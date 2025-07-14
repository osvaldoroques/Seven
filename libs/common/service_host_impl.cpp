#include "service_host.hpp"
#include "messages.pb.h"
#include <unordered_map>  // For trace context headers

// Static instance for signal handler
ServiceHost* ServiceHost::instance_ = nullptr;

// Signal handler for graceful shutdown
static void signal_handler(int signal) {
    std::cout << "\n🛑 Received signal " << signal << ", initiating graceful shutdown..." << std::endl;
    if (ServiceHost::instance_) {
        ServiceHost::instance_->stop();
    }
}

ServiceHost::~ServiceHost() {
    shutdown();
}

void ServiceHost::shutdown() {
    if (!running_) return;  // Already shut down
    
    std::cout << "🛑 Shutting down ServiceHost..." << std::endl;
    
    // Stop accepting new work
    running_ = false;
    
    // Stop configuration watching
    try {
        config_.stopWatch();
        std::cout << "✅ Configuration watcher stopped" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "⚠️ Error stopping config watcher: " << e.what() << std::endl;
    }
    
    // Shutdown thread pool (wait for current tasks to complete)
    thread_pool_.shutdown();
    std::cout << "✅ Thread pool shutdown completed" << std::endl;
    
    // Close NATS connections
    if (js_) {
        jsCtx_Destroy(js_);
        js_ = nullptr;
        std::cout << "✅ JetStream context destroyed" << std::endl;
    }
    
    if (conn_) {
        natsConnection_Close(conn_);
        natsConnection_Destroy(conn_);
        conn_ = nullptr;
        std::cout << "✅ NATS connection closed" << std::endl;
    }
    
    std::cout << "✅ ServiceHost shutdown completed" << std::endl;
}

void ServiceHost::shutdown_with_timeout(std::chrono::milliseconds timeout) {
    if (!running_) return;  // Already shut down
    
    std::cout << "🛑 Initiating graceful shutdown with " << timeout.count() << "ms timeout..." << std::endl;
    
    auto start_time = std::chrono::steady_clock::now();
    
    // Stop accepting new work
    running_ = false;
    
    // Start shutdown in a separate thread
    std::thread shutdown_thread([this]() {
        shutdown();
    });
    
    // Wait for shutdown to complete or timeout
    if (shutdown_thread.joinable()) {
        if (std::chrono::steady_clock::now() - start_time < timeout) {
            shutdown_thread.join();
            std::cout << "✅ Graceful shutdown completed within timeout" << std::endl;
        } else {
            shutdown_thread.detach();
            std::cout << "⚠️ Graceful shutdown timed out, forcing termination" << std::endl;
        }
    }
}

void ServiceHost::setup_signal_handlers() {
    instance_ = this;
    
    // Register signal handlers for graceful shutdown
    std::signal(SIGINT, signal_handler);   // Ctrl+C
    std::signal(SIGTERM, signal_handler);  // Termination request
    
    std::cout << "✅ Signal handlers registered (SIGINT, SIGTERM)" << std::endl;
}

void ServiceHost::init_nats(const std::string& nats_url) {
    std::string effective_url = nats_url;
    
    // Use configuration NATS URL if available and no explicit URL provided
    if (config_ && nats_url == "nats://localhost:4222") {
        auto config_nats_url = config_.get<std::string>("nats.url", "");
        if (!config_nats_url.empty()) {
            effective_url = config_nats_url;
        }
    }
    
    status_ = natsConnection_ConnectTo(&conn_, effective_url.c_str());
    if (status_ != NATS_OK) {
        std::cerr << "❌ NATS connection failed: " << natsStatus_GetText(status_) << std::endl;
        exit(1);
    }
    std::cout << "✅ Connected to NATS: " << effective_url << std::endl;
}

void ServiceHost::init_jetstream() {
    status_ = natsConnection_JetStream(&js_, conn_, NULL);
    if (status_ != NATS_OK) {
        std::cerr << "❌ JetStream initialization failed: " << natsStatus_GetText(status_) << std::endl;
        exit(1);
    }
    std::cout << "✅ JetStream initialized." << std::endl;
}

// 🚀 Hot-path function pointer optimization (zero-branching)

void ServiceHost::enable_tracing() {
    tracing_enabled_ = true;
    publish_broadcast_impl_ = &ServiceHost::publish_broadcast_traced;
    publish_point_to_point_impl_ = &ServiceHost::publish_point_to_point_traced;
}

void ServiceHost::disable_tracing() {
    tracing_enabled_ = false;
    publish_broadcast_impl_ = &ServiceHost::publish_broadcast_fast;
    publish_point_to_point_impl_ = &ServiceHost::publish_point_to_point_fast;
}

// Hot-path method with function pointer dispatch (zero overhead)
void ServiceHost::publish_broadcast(const google::protobuf::Message &message) {
    (this->*publish_broadcast_impl_)(message);
}

void ServiceHost::publish_point_to_point(const std::string &target_uid, const google::protobuf::Message &message) {
    (this->*publish_point_to_point_impl_)(target_uid, message);
}

// Non-traced implementation (maximum performance)
void ServiceHost::publish_broadcast_fast(const google::protobuf::Message &message) {
    std::lock_guard<std::mutex> lock(publish_mutex_);
    
    if (!conn_) {
        std::cerr << "❌ NATS connection not initialized" << std::endl;
        return;
    }

    std::string type_name = message.GetTypeName();
    std::string data;
    if (!message.SerializeToString(&data)) {
        std::cerr << "❌ Failed to serialize message of type: " << type_name << std::endl;
        return;
    }

    std::string subject = "broadcast." + type_name;
    natsStatus status = natsConnection_Publish(conn_, subject.c_str(), data.c_str(), data.length());
    if (status != NATS_OK) {
        std::cerr << "❌ Failed to publish broadcast message: " << natsStatus_GetText(status) << std::endl;
    }
}

void ServiceHost::publish_point_to_point_fast(const std::string &target_uid, const google::protobuf::Message &message) {
    std::lock_guard<std::mutex> lock(publish_mutex_);
    
    if (!conn_) {
        std::cerr << "❌ NATS connection not initialized" << std::endl;
        return;
    }

    std::string type_name = message.GetTypeName();
    std::string data;
    if (!message.SerializeToString(&data)) {
        std::cerr << "❌ Failed to serialize message of type: " << type_name << std::endl;
        return;
    }

    std::string subject = "p2p." + target_uid + "." + type_name;
    natsStatus status = natsConnection_Publish(conn_, subject.c_str(), data.c_str(), data.length());
    if (status != NATS_OK) {
        std::cerr << "❌ Failed to publish p2p message: " << natsStatus_GetText(status) << std::endl;
    }
}

// Traced implementation (with OpenTelemetry overhead)
void ServiceHost::publish_broadcast_traced(const google::protobuf::Message &message) {
#ifdef HAVE_OPENTELEMETRY
    // Create span for this operation
    auto tracer = opentelemetry::trace::Provider::GetTracerProvider()->GetTracer("nats_service");
    auto span = tracer->StartSpan("publish_broadcast");
    
    // Set span attributes
    span->SetAttribute("message.type", message.GetTypeName());
    span->SetAttribute("publish.mode", "broadcast");
    span->SetAttribute("service.uid", uid_);
#endif

    std::lock_guard<std::mutex> lock(publish_mutex_);
    
    if (!conn_) {
        std::cerr << "❌ NATS connection not initialized" << std::endl;
#ifdef HAVE_OPENTELEMETRY
        span->SetStatus(opentelemetry::trace::StatusCode::kError, "NATS connection not initialized");
        span->End();
#endif
        return;
    }

    std::string type_name = message.GetTypeName();
    std::string data;
    if (!message.SerializeToString(&data)) {
        std::cerr << "❌ Failed to serialize message of type: " << type_name << std::endl;
#ifdef HAVE_OPENTELEMETRY
        span->SetStatus(opentelemetry::trace::StatusCode::kError, "Message serialization failed");
        span->End();
#endif
        return;
    }

    std::string subject = "broadcast." + type_name;

#ifdef HAVE_OPENTELEMETRY
    // Create NATS message with tracing headers
    natsMsg *natsmsg = nullptr;
    natsStatus status = natsMsg_Create(&natsmsg, subject.c_str(), nullptr, data.c_str(), data.length());
    
    if (status == NATS_OK) {
        // Inject trace context into NATS headers
        auto context = span->GetContext();
        if (context.IsValid()) {
            opentelemetry::trace::TraceId trace_id = context.trace_id();
            opentelemetry::trace::SpanId span_id = context.span_id();
            
            char trace_id_hex[33];
            char span_id_hex[17];
            trace_id.ToLowerBase16(trace_id_hex);
            span_id.ToLowerBase16(span_id_hex);
            trace_id_hex[32] = '\0';
            span_id_hex[16] = '\0';
            
            std::string traceparent = "00-" + std::string(trace_id_hex) + "-" + std::string(span_id_hex) + "-01";
            
            status = natsMsgHeader_Add(natsmsg, "traceparent", traceparent.c_str());
            if (status != NATS_OK) {
                std::cerr << "⚠️ Failed to add traceparent header: " << natsStatus_GetText(status) << std::endl;
            }
        }
        
        status = natsConnection_PublishMsg(conn_, natsmsg);
        natsMsg_Destroy(natsmsg);
    }
    
    if (status != NATS_OK) {
        std::cerr << "❌ Failed to publish broadcast message: " << natsStatus_GetText(status) << std::endl;
        span->SetStatus(opentelemetry::trace::StatusCode::kError, natsStatus_GetText(status));
    } else {
        span->SetStatus(opentelemetry::trace::StatusCode::kOk);
    }
    
    span->End();
#else
    // Fallback to simple publish without tracing
    natsStatus status = natsConnection_Publish(conn_, subject.c_str(), data.c_str(), data.length());
    if (status != NATS_OK) {
        std::cerr << "❌ Failed to publish broadcast message: " << natsStatus_GetText(status) << std::endl;
    }
#endif
}

void ServiceHost::publish_point_to_point_traced(const std::string &target_uid, const google::protobuf::Message &message) {
#ifdef HAVE_OPENTELEMETRY
    // Create span for this operation
    auto tracer = opentelemetry::trace::Provider::GetTracerProvider()->GetTracer("nats_service");
    auto span = tracer->StartSpan("publish_point_to_point");
    
    // Set span attributes
    span->SetAttribute("message.type", message.GetTypeName());
    span->SetAttribute("publish.mode", "point_to_point");
    span->SetAttribute("target.uid", target_uid);
    span->SetAttribute("service.uid", uid_);
#endif

    std::lock_guard<std::mutex> lock(publish_mutex_);
    
    if (!conn_) {
        std::cerr << "❌ NATS connection not initialized" << std::endl;
#ifdef HAVE_OPENTELEMETRY
        span->SetStatus(opentelemetry::trace::StatusCode::kError, "NATS connection not initialized");
        span->End();
#endif
        return;
    }

    std::string type_name = message.GetTypeName();
    std::string data;
    if (!message.SerializeToString(&data)) {
        std::cerr << "❌ Failed to serialize message of type: " << type_name << std::endl;
#ifdef HAVE_OPENTELEMETRY
        span->SetStatus(opentelemetry::trace::StatusCode::kError, "Message serialization failed");
        span->End();
#endif
        return;
    }

    std::string subject = "p2p." + target_uid + "." + type_name;

#ifdef HAVE_OPENTELEMETRY
    // Create NATS message with tracing headers
    natsMsg *natsmsg = nullptr;
    natsStatus status = natsMsg_Create(&natsmsg, subject.c_str(), nullptr, data.c_str(), data.length());
    
    if (status == NATS_OK) {
        // Inject trace context into NATS headers
        auto context = span->GetContext();
        if (context.IsValid()) {
            opentelemetry::trace::TraceId trace_id = context.trace_id();
            opentelemetry::trace::SpanId span_id = context.span_id();
            
            char trace_id_hex[33];
            char span_id_hex[17];
            trace_id.ToLowerBase16(trace_id_hex);
            span_id.ToLowerBase16(span_id_hex);
            trace_id_hex[32] = '\0';
            span_id_hex[16] = '\0';
            
            std::string traceparent = "00-" + std::string(trace_id_hex) + "-" + std::string(span_id_hex) + "-01";
            
            status = natsMsgHeader_Add(natsmsg, "traceparent", traceparent.c_str());
            if (status != NATS_OK) {
                std::cerr << "⚠️ Failed to add traceparent header: " << natsStatus_GetText(status) << std::endl;
            }
        }
        
        status = natsConnection_PublishMsg(conn_, natsmsg);
        natsMsg_Destroy(natsmsg);
    }
    
    if (status != NATS_OK) {
        std::cerr << "❌ Failed to publish p2p message: " << natsStatus_GetText(status) << std::endl;
        span->SetStatus(opentelemetry::trace::StatusCode::kError, natsStatus_GetText(status));
    } else {
        span->SetStatus(opentelemetry::trace::StatusCode::kOk);
    }
    
    span->End();
#else
    // Fallback to simple publish without tracing
    natsStatus status = natsConnection_Publish(conn_, subject.c_str(), data.c_str(), data.length());
    if (status != NATS_OK) {
        std::cerr << "❌ Failed to publish p2p message: " << natsStatus_GetText(status) << std::endl;
    }
#endif
}

void ServiceHost::subscribe_broadcast(const std::string& type_name) {
    std::string subject = "system.broadcast." + type_name;
    natsSubscription* sub = nullptr;

    status_ = natsConnection_Subscribe(&sub, conn_, subject.c_str(),
      [](natsConnection*, natsSubscription*, natsMsg* msg, void* closure) {
        auto* self = static_cast<ServiceHost*>(closure);
        std::string subj(natsMsg_GetSubject(msg));
        std::string payload(natsMsg_GetData(msg), natsMsg_GetDataLength(msg));
        std::string prefix = "system.broadcast.";
        std::string tn = subj.substr(prefix.size());
        self->receive_message(tn, payload);
        natsMsg_Destroy(msg);
      }, this);

    if (status_ == NATS_OK)
        std::cout << "📡 Subscribed to broadcast: " << subject << std::endl;
    else
        std::cerr << "❌ Failed to subscribe broadcast: "
                  << natsStatus_GetText(status_) << std::endl;
}

void ServiceHost::subscribe_point_to_point(const std::string& type_name) {
    const std::string subject = "system.direct." + uid_ + "." + type_name;
    natsSubscription* sub = nullptr;

    status_ = natsConnection_Subscribe(&sub, conn_, subject.c_str(),
        [](natsConnection*, natsSubscription*, natsMsg* msg, void* closure) {
            auto* self = static_cast<ServiceHost*>(closure);
            std::string subject_str = natsMsg_GetSubject(msg);
            // Extract the full type name (including namespace) from subject
            // Format: system.direct.svc-portfolio-001.Trevor.HealthCheckRequest
            std::string prefix = "system.direct." + self->uid_ + ".";
            std::string extracted_type_name = subject_str.substr(prefix.length());
            std::string payload(natsMsg_GetData(msg), natsMsg_GetDataLength(msg));
            self->receive_message(extracted_type_name, payload);
            natsMsg_Destroy(msg);
        }, this);

    if (status_ == NATS_OK) {
        std::cout << "📡 Subscribed to point-to-point: " << subject << std::endl;
    } else {
        std::cerr << "❌ Failed to subscribe: " << natsStatus_GetText(status_) << std::endl;
    }
}

void ServiceHost::subscribe_broadcast_V2(const std::string& type_name) {
    std::string subject = "system.broadcast." + type_name;
    natsSubscription* sub = nullptr;

    status_ = natsConnection_Subscribe(&sub, conn_, subject.c_str(),
        [](natsConnection*, natsSubscription*, natsMsg* msg, void* closure) {
            auto* self = static_cast<ServiceHost*>(closure);
            
            // 1️⃣ Extract trace context from NATS headers
            std::unordered_map<std::string, std::string> headers;
            #ifdef HAVE_OPENTELEMETRY
            if (msg->hdr && msg->hdr->count > 0) {
                for (int i = 0; i < msg->hdr->count; ++i) {
                    if (msg->hdr->keys[i] && msg->hdr->values[i]) {
                        headers[msg->hdr->keys[i]] = msg->hdr->values[i];
                    }
                }
            }
            auto parent_context = OpenTelemetryIntegration::extract_trace_context(headers);
            #endif
            
            // 2️⃣ Start child span for receiving
            std::string subj(natsMsg_GetSubject(msg));
            std::string prefix = "system.broadcast.";
            std::string type_name = subj.substr(prefix.size());
            
            #ifdef HAVE_OPENTELEMETRY
            auto span = OpenTelemetryIntegration::start_span(
                "receive:" + type_name, parent_context);
            #endif
            
            // 3️⃣ Process the message
            std::string payload(natsMsg_GetData(msg), natsMsg_GetDataLength(msg));
            self->receive_message(type_name, payload);
            
            // 4️⃣ End span
            #ifdef HAVE_OPENTELEMETRY
            OpenTelemetryIntegration::end_span(span);
            #endif
            
            natsMsg_Destroy(msg);
        }, this);

    if (status_ == NATS_OK)
        std::cout << "📡 Subscribed to broadcast V2 (with tracing): " << subject << std::endl;
    else
        std::cerr << "❌ Failed to subscribe broadcast V2: "
                  << natsStatus_GetText(status_) << std::endl;
}

void ServiceHost::subscribe_point_to_point_V2(const std::string& type_name) {
    const std::string subject = "system.direct." + uid_ + "." + type_name;
    natsSubscription* sub = nullptr;

    status_ = natsConnection_Subscribe(&sub, conn_, subject.c_str(),
        [](natsConnection*, natsSubscription*, natsMsg* msg, void* closure) {
            auto* self = static_cast<ServiceHost*>(closure);
            
            // 1️⃣ Extract trace context from NATS headers
            std::unordered_map<std::string, std::string> headers;
            #ifdef HAVE_OPENTELEMETRY
            if (msg->hdr && msg->hdr->count > 0) {
                for (int i = 0; i < msg->hdr->count; ++i) {
                    if (msg->hdr->keys[i] && msg->hdr->values[i]) {
                        headers[msg->hdr->keys[i]] = msg->hdr->values[i];
                    }
                }
            }
            auto parent_context = OpenTelemetryIntegration::extract_trace_context(headers);
            #endif
            
            // 2️⃣ Start child span for receiving
            std::string subject_str = natsMsg_GetSubject(msg);
            std::string prefix = "system.direct." + self->uid_ + ".";
            std::string extracted_type_name = subject_str.substr(prefix.length());
            
            #ifdef HAVE_OPENTELEMETRY
            auto span = OpenTelemetryIntegration::start_span(
                "receive:" + extracted_type_name, parent_context);
            #endif
            
            // 3️⃣ Process the message
            std::string payload(natsMsg_GetData(msg), natsMsg_GetDataLength(msg));
            self->receive_message(extracted_type_name, payload);
            
            // 4️⃣ End span
            #ifdef HAVE_OPENTELEMETRY
            OpenTelemetryIntegration::end_span(span);
            #endif
            
            natsMsg_Destroy(msg);
        }, this);

    if (status_ == NATS_OK) {
        std::cout << "📡 Subscribed to point-to-point V2 (with tracing): " << subject << std::endl;
    } else {
        std::cerr << "❌ Failed to subscribe V2: " << natsStatus_GetText(status_) << std::endl;
    }
}

// Explicit template instantiation
template void ServiceHost::register_message<Trevor::HealthCheckRequest>(
    MessageRouting routing,
    std::function<void(const Trevor::HealthCheckRequest&)> handler);

template void ServiceHost::register_message<Trevor::HealthCheckResponse>(
    MessageRouting routing,
    std::function<void(const Trevor::HealthCheckResponse&)> handler);

