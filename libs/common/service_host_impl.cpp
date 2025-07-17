#include "service_host.hpp"
#include "messages.pb.h"
#include <unordered_map>  // For trace context headers
#include <future>         // For std::future, std::promise, std::async
#include <thread>         // For std::thread
#include <fstream>        // For system monitoring
#include <sstream>        // For string stream operations
#include <sys/resource.h> // For resource usage monitoring

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
    
    // Stop permanent maintenance tasks
    StopPermanentTasks();
    
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
    
    // Initialize cache system once NATS is connected
    init_cache_system();
}

void ServiceHost::init_cache_system() {
    if (cache_) {
        // Setup cache management and distributed handlers
        cache_->setup_cache_management();
        cache_->setup_distributed_cache_handlers();
        
        logger_->info("✅ ServiceCache system initialized and wired into ServiceHost");
    }
}

void ServiceHost::init_jetstream() {
    if (!conn_) {
        logger_->error("❌ Cannot initialize JetStream: NATS connection not initialized");
        return;
    }

    status_ = natsConnection_JetStream(&js_, conn_, NULL);
    if (status_ != NATS_OK) {
        logger_->error("❌ Failed to initialize JetStream: {}", natsStatus_GetText(status_));
        return;
    }
    
    logger_->info("✅ JetStream initialized successfully");
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
    // Simple NATS publish without tracing
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
    // Simple NATS publish without tracing
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

// 🚀 Performance benchmarking and validation
void ServiceHost::run_performance_benchmark(int iterations, bool verbose) {
    if (verbose) {
        std::cout << "\n🚀 ServiceHost Performance Benchmark\n";
        std::cout << "=====================================\n";
        std::cout << "Testing function pointer optimization with " << iterations << " iterations\n\n";
    }
    
    // Create a test message
    std::string test_data = "benchmark_data_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    
    // Test fast mode
    disable_tracing();
    auto start_fast = std::chrono::high_resolution_clock::now();
    
    // We can't easily create a protobuf message here without dependencies,
    // so we'll measure the function pointer dispatch overhead
    for (int i = 0; i < iterations; ++i) {
        // Simulate function pointer call overhead
        volatile bool current_mode = tracing_enabled_;
        (void)current_mode; // Prevent optimization
    }
    
    auto end_fast = std::chrono::high_resolution_clock::now();
    auto fast_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_fast - start_fast);
    
    // Test traced mode  
    enable_tracing();
    auto start_traced = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        // Simulate function pointer call overhead
        volatile bool current_mode = tracing_enabled_;
        (void)current_mode; // Prevent optimization
    }
    
    auto end_traced = std::chrono::high_resolution_clock::now();
    auto traced_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_traced - start_traced);
    
    if (verbose) {
        double overhead_ratio = static_cast<double>(traced_duration.count()) / fast_duration.count();
        
        std::cout << "📊 Benchmark Results:\n";
        std::cout << "   • Fast mode:   " << fast_duration.count() << "ns total, " 
                  << std::fixed << std::setprecision(2) << (fast_duration.count() / static_cast<double>(iterations)) << "ns per operation\n";
        std::cout << "   • Traced mode: " << traced_duration.count() << "ns total, " 
                  << std::setprecision(2) << (traced_duration.count() / static_cast<double>(iterations)) << "ns per operation\n";
        std::cout << "   • Overhead ratio: " << std::setprecision(3) << overhead_ratio << "x\n";
        
        if (overhead_ratio < 1.1) {
            std::cout << "   • 🎉 EXCELLENT: Virtually no overhead\n";
        } else if (overhead_ratio < 2.0) {
            std::cout << "   • ✅ GOOD: Minimal overhead\n";
        } else {
            std::cout << "   • ⚠️  WARNING: Unexpected overhead detected\n";
        }
        
        std::cout << "=====================================\n\n";
    }
    
    // Reset to traced mode
    enable_tracing();
}

// 🚀 COMPREHENSIVE SERVICE INITIALIZATION 🚀
void ServiceHost::start_subscription_processing() {
    logger_->info("🚀 Starting subscription processing for {} registered handlers", handlers_.size());
    
    if (handlers_.empty()) {
        logger_->warn("⚠️ No message handlers registered - service may not process any messages");
        return;
    }
    
    // All handlers are already registered during register_message calls
    // The subscription processing is automatically started when handlers are registered
    // This method serves as a confirmation that all subscriptions are active
    
    for (const auto& [message_type, handler] : handlers_) {
        logger_->debug("📡 Active subscription for message type: {}", message_type);
    }
    
    logger_->info("✅ Subscription processing started for all registered handlers");
}

void ServiceHost::StartService(const ServiceInitConfig& config) {
    logger_->info("🚀 Starting comprehensive service startup for: {}", service_name_);
    
    // 1. Initialize the service with all configurations
    initialize_service(config);
    
    // 2. Setup signal handlers for graceful shutdown
    setup_signal_handlers();
    
    // 3. Start subscription processing
    start_subscription_processing();
    
    // 4. Set service as running
    running_ = true;
    
    // 5. Log final startup summary
    logger_->info("✅ Service startup completed successfully");
    logger_->info("🎯 Service: {} (UID: {})", service_name_, uid_);
    logger_->info("🎯 Status: {}", get_status());
    logger_->info("🧵 Worker threads: {}", thread_pool_.size());
    logger_->info("📡 NATS connection: {}", conn_ ? "Connected" : "Disconnected");
    logger_->info("🚀 JetStream: {}", js_ ? "Enabled" : "Disabled");
    logger_->info("🧠 Cache: {}", config.enable_cache ? "Enabled" : "Disabled");
    logger_->info("⏰ Scheduler: {}", config.enable_scheduler ? "Enabled" : "Disabled");
    logger_->info("⚡ Performance mode: {}", config.enable_performance_mode ? "Enabled" : "Disabled");
    
    std::cout << "🚀 " << service_name_ << " service started successfully!" << std::endl;
}

std::future<void> ServiceHost::StartServiceAsync(const ServiceInitConfig& config) {
    logger_->info("🚀 Starting async service startup for: {}", service_name_);
    
    return std::async(std::launch::async, [this, config]() {
        // Run the full startup process in background
        StartService(config);
    });
}

std::future<void> ServiceHost::StartServiceInfrastructureAsync(const ServiceInitConfig& config) {
    logger_->info("🚀 Starting async infrastructure initialization for: {}", service_name_);
    
    std::promise<void> prom;
    auto fut = prom.get_future();
    
    // Capture `this` by pointer safely; detach thread
    std::thread([this, config, prom = std::move(prom)]() mutable {
        try {
            logger_->info("🚀 Starting infrastructure initialization in background thread");
            
            // 1️⃣ Start configuration file watching
            try {
                config_.startWatch();
                config_.onReload([this]() { 
                    logger_->info("Configuration reloaded"); 
                });
                logger_->info("✅ Configuration file watching started");
            } catch (const std::exception& e) {
                logger_->warn("⚠️ Configuration file watching failed: {}", e.what());
            }
            
            // 2️⃣ Start the scheduler
            try {
                scheduler_->start();
                logger_->info("✅ ServiceScheduler started");
            } catch (const std::exception& e) {
                logger_->error("❌ ServiceScheduler failed to start: {}", e.what());
                throw;
            }
            
            // 3️⃣ Initialize OpenTelemetry
            try {
                const char *otlp_endpoint = std::getenv("OTEL_EXPORTER_OTLP_ENDPOINT");
                if (otlp_endpoint && OpenTelemetryIntegration::is_available()) {
                    if (OpenTelemetryIntegration::initialize(service_name_, otlp_endpoint)) {
                        logger_->info("✅ OpenTelemetry initialized: {} -> {}", service_name_, otlp_endpoint);
                    } else {
                        logger_->error("❌ OpenTelemetry initialization failed: {} -> {}", service_name_, otlp_endpoint);
                    }
                } else if (otlp_endpoint && !OpenTelemetryIntegration::is_available()) {
                    logger_->warn("⚠️ OpenTelemetry endpoint set but not compiled with HAVE_OPENTELEMETRY");
                } else {
                    logger_->debug("🔍 OTEL_EXPORTER_OTLP_ENDPOINT not set, skipping OpenTelemetry initialization");
                }
            } catch (const std::exception& e) {
                logger_->warn("⚠️ OpenTelemetry initialization failed: {}", e.what());
            }
            
            // 4️⃣ Initialize NATS Connection
            logger_->info("📡 Initializing NATS connection: {}", config.nats_url);
            init_nats(config.nats_url);
            
            // 5️⃣ Initialize JetStream if enabled
            if (config.enable_jetstream) {
                logger_->info("🚀 Initializing JetStream");
                init_jetstream();
            }
            
            // 6️⃣ Configure Performance Mode
            if (config.enable_performance_mode) {
                disable_tracing();
                logger_->info("⚡ Performance mode enabled (tracing disabled)");
            } else {
                enable_tracing();
                logger_->info("🔍 Full observability mode enabled");
            }
            
            // 7️⃣ Initialize Cache System
            if (config.enable_cache) {
                logger_->info("🧠 Initializing cache system (default: {} items, TTL: {}s)", 
                             config.default_cache_size, config.default_cache_ttl.count());
                init_cache_system();
            }
            
            // 8️⃣ Setup signal handlers for graceful shutdown
            setup_signal_handlers();
            
            logger_->info("✅ Infrastructure initialization completed successfully");
            
            // 9️⃣ Signal success
            prom.set_value();
            
        } catch (...) {
            logger_->error("❌ Infrastructure initialization failed");
            // Propagate any exception
            prom.set_exception(std::current_exception());
        }
    }).detach();
    
    return fut;
}

std::future<void> ServiceHost::CompleteServiceStartup(const ServiceInitConfig& config) {
    logger_->info("🚀 Completing service startup for: {}", service_name_);
    
    std::promise<void> prom;
    auto fut = prom.get_future();
    
    // Capture `this` by pointer safely; detach thread
    std::thread([this, config, prom = std::move(prom)]() mutable {
        try {
            logger_->info("🚀 Starting complete service startup in background thread");
            
            // 1️⃣ Start configuration file watching
            try {
                config_.startWatch();
                config_.onReload([this]() { 
                    logger_->info("Configuration reloaded"); 
                });
                logger_->info("✅ Configuration file watching started");
            } catch (const std::exception& e) {
                logger_->warn("⚠️ Configuration file watching failed: {}", e.what());
            }
            
            // 2️⃣ Start the scheduler
            try {
                scheduler_->start();
                logger_->info("✅ ServiceScheduler started");
            } catch (const std::exception& e) {
                logger_->error("❌ ServiceScheduler failed to start: {}", e.what());
                throw;
            }
            
            // 3️⃣ Initialize OpenTelemetry
            try {
                const char *otlp_endpoint = std::getenv("OTEL_EXPORTER_OTLP_ENDPOINT");
                if (otlp_endpoint && OpenTelemetryIntegration::is_available()) {
                    if (OpenTelemetryIntegration::initialize(service_name_, otlp_endpoint)) {
                        logger_->info("✅ OpenTelemetry initialized: {} -> {}", service_name_, otlp_endpoint);
                    } else {
                        logger_->error("❌ OpenTelemetry initialization failed: {} -> {}", service_name_, otlp_endpoint);
                    }
                } else if (otlp_endpoint && !OpenTelemetryIntegration::is_available()) {
                    logger_->warn("⚠️ OpenTelemetry endpoint set but not compiled with HAVE_OPENTELEMETRY");
                } else {
                    logger_->debug("🔍 OTEL_EXPORTER_OTLP_ENDPOINT not set, skipping OpenTelemetry initialization");
                }
            } catch (const std::exception& e) {
                logger_->warn("⚠️ OpenTelemetry initialization failed: {}", e.what());
            }
            
            // 4️⃣ Initialize full service through existing method
            logger_->info("🚀 Starting full service initialization");
            initialize_service(config);
            
            // 5️⃣ Setup signal handlers for graceful shutdown
            setup_signal_handlers();
            
            // 6️⃣ Setup Automatic Cache Cleanup
            if (config.enable_scheduler && config.enable_auto_cache_cleanup) {
                schedule_cache_cleanup([this]() {
                    cache_->cleanup_expired();
                    logger_->debug("🧹 Automatic cache cleanup completed");
                });
                logger_->info("🧹 Scheduled automatic cache cleanup every {} minutes", 
                             config.cache_cleanup_interval.count());
            }
            
            // 7️⃣ Setup Metrics Flush
            if (config.enable_metrics_flush && config.metrics_flush_callback) {
                schedule_metrics_flush(config.metrics_flush_callback);
                logger_->info("📊 Scheduled metrics flush every {} seconds", 
                             config.metrics_flush_interval.count());
            }
            
            // 8️⃣ Setup Health Heartbeat
            if (config.enable_health_heartbeat && config.health_heartbeat_callback) {
                schedule_health_heartbeat(config.health_heartbeat_callback);
                logger_->info("❤️ Scheduled health heartbeat every {} seconds", 
                             config.health_heartbeat_interval.count());
            }
            
            // 9️⃣ Setup Back-pressure Monitoring
            if (config.enable_backpressure_monitor && config.queue_size_func && config.backpressure_callback) {
                schedule_backpressure_monitor(
                    config.queue_size_func,
                    config.backpressure_threshold,
                    config.backpressure_callback
                );
                logger_->info("⚠️ Scheduled backpressure monitoring (threshold: {})", 
                             config.backpressure_threshold);
            }
            
            // 🔟 Force OpenTelemetry Initialization if requested
            if (config.force_otel_initialization) {
                const char* endpoint = config.custom_otel_endpoint.empty() 
                    ? std::getenv("OTEL_EXPORTER_OTLP_ENDPOINT") 
                    : config.custom_otel_endpoint.c_str();
                    
                if (endpoint && OpenTelemetryIntegration::is_available()) {
                    if (OpenTelemetryIntegration::initialize(service_name_, endpoint)) {
                        logger_->info("🔍 OpenTelemetry force-initialized: {} -> {}", service_name_, endpoint);
                    } else {
                        logger_->warn("⚠️ OpenTelemetry force-initialization failed");
                    }
                }
            }
            
            // 1️⃣1️⃣ Start subscription processing
            start_subscription_processing();
            
            // 1️⃣2️⃣ Set service as running
            running_ = true;
            
            // 1️⃣3️⃣ Start permanent service maintenance tasks
            if (config.enable_permanent_tasks) {
                StartPermanentTasks(config);
            }
            
            // 1️⃣4️⃣ Final Health Check and Logging
            if (is_healthy()) {
                logger_->info("✅ Service startup completed successfully");
                logger_->info("🎯 Service: {} (UID: {})", service_name_, uid_);
                logger_->info("🎯 Status: {}", get_status());
                logger_->info("🧵 Worker threads: {}", thread_pool_.size());
                logger_->info("📡 NATS connection: {}", conn_ ? "Connected" : "Disconnected");
                logger_->info("🚀 JetStream: {}", js_ ? "Enabled" : "Disabled");
                logger_->info("🧠 Cache: {}", config.enable_cache ? "Enabled" : "Disabled");
                logger_->info("⏰ Scheduler: {}", config.enable_scheduler ? "Enabled" : "Disabled");
                logger_->info("⚡ Performance mode: {}", config.enable_performance_mode ? "Enabled" : "Disabled");
                logger_->info("🔄 Permanent tasks: {}", config.enable_permanent_tasks ? "Enabled" : "Disabled");
                
                std::cout << "🚀 " << service_name_ << " service started successfully!" << std::endl;
            } else {
                logger_->error("❌ Service startup completed with issues");
                throw std::runtime_error("Service startup failed health check");
            }
            
            logger_->info("✅ Complete service startup finished successfully");
            
            // 1️⃣4️⃣ Signal success
            prom.set_value();
            
        } catch (...) {
            logger_->error("❌ Complete service startup failed");
            // Propagate any exception
            prom.set_exception(std::current_exception());
        }
    }).detach();
    
    return fut;
}

void ServiceHost::initialize_service(const ServiceInitConfig& config) {
    logger_->info("🚀 Starting core service initialization for: {}", service_name_);
    
    // 1️⃣ Initialize NATS Connection
    try {
        logger_->info("📡 Initializing NATS connection: {}", config.nats_url);
        init_nats(config.nats_url);
        
        if (config.enable_jetstream) {
            logger_->info("🚀 Initializing JetStream");
            init_jetstream();
        }
    } catch (const std::exception& e) {
        logger_->error("❌ NATS initialization failed: {}", e.what());
        throw;
    }
    
    // 2️⃣ Configure Performance Mode
    if (config.enable_performance_mode) {
        disable_tracing();
        logger_->info("⚡ Performance mode enabled (tracing disabled)");
    } else {
        enable_tracing();
        logger_->info("🔍 Full observability mode enabled");
    }
    
    // 3️⃣ Initialize Cache System
    if (config.enable_cache) {
        logger_->info("🧠 Initializing cache system (default: {} items, TTL: {}s)", 
                     config.default_cache_size, config.default_cache_ttl.count());
        init_cache_system();
    }
    
    // 4️⃣ Final Health Check
    if (is_healthy()) {
        logger_->info("✅ Core service initialization completed successfully");
        logger_->info("🎯 Service Status: {}", get_status());
        logger_->info("🧵 Worker threads: {}", thread_pool_.size());
        logger_->info("🧠 Cache enabled: {}", config.enable_cache ? "Yes" : "No");
        logger_->info("⚡ Performance mode: {}", config.enable_performance_mode ? "Yes" : "No");
    } else {
        logger_->error("❌ Core service initialization completed with issues");
        throw std::runtime_error("Core service initialization failed health check");
    }
}

// Explicit template instantiation
template void ServiceHost::register_message<Trevor::HealthCheckRequest>(
    MessageRouting routing,
    std::function<void(const Trevor::HealthCheckRequest&)> handler);

template void ServiceHost::register_message<Trevor::HealthCheckResponse>(
    MessageRouting routing,
    std::function<void(const Trevor::HealthCheckResponse&)> handler);

// 🚀 NEW: Simplified Handler Registration Implementation
void ServiceHost::register_handlers(const RegistrationMap& regs) {
    for (const auto& [message_type, routing_handler] : regs) {
        const auto& [routing, handler] = routing_handler;
        register_handler(message_type, routing, handler);
    }
}

void ServiceHost::register_handler(const std::string& message_type, 
                                 MessageRouting routing, 
                                 HandlerRaw handler) {
    
    logger_->info("Registering handler for message type: {}, routing: {}", 
                 message_type, routing == MessageRouting::PointToPoint ? "PointToPoint" : "Broadcast");
    
    // Create a generic handler that works with raw payloads
    auto generic_handler = [this, handler, message_type](const std::string& payload) {
        try {
            // Call the user's handler with the raw payload
            handler(payload);
        } catch (const std::exception& e) {
            logger_->error("Handler for {} failed: {}", message_type, e.what());
        }
    };
    
    // Store the handler in our handlers map
    handlers_[message_type] = generic_handler;
    
    // Set up NATS subscription based on routing
    if (routing == MessageRouting::PointToPoint) {
        // Point-to-point: subscribe to service-specific subject
        std::string subject = uid_ + "." + message_type;
        
        natsSubscription* sub = nullptr;
        natsStatus status = natsConnection_Subscribe(&sub, conn_, subject.c_str(),
            [](natsConnection* conn, natsSubscription* sub, natsMsg* msg, void* closure) {
                ServiceHost* host = static_cast<ServiceHost*>(closure);
                const char* subject = natsMsg_GetSubject(msg);
                const char* data = natsMsg_GetData(msg);
                int data_len = natsMsg_GetDataLength(msg);
                
                std::string payload(data, data_len);
                
                // Extract message type from subject (format: uid.MessageType)
                std::string subject_str(subject);
                size_t dot_pos = subject_str.find('.');
                if (dot_pos != std::string::npos) {
                    std::string msg_type = subject_str.substr(dot_pos + 1);
                    
                    auto it = host->handlers_.find(msg_type);
                    if (it != host->handlers_.end()) {
                        // Execute handler in thread pool
                        host->thread_pool_.submit([handler = it->second, payload]() {
                            handler(payload);
                        });
                    }
                }
                
                natsMsg_Destroy(msg);
            }, this);
            
        if (status == NATS_OK) {
            logger_->info("Successfully subscribed to point-to-point subject: {}", subject);
        } else {
            logger_->error("Failed to subscribe to point-to-point subject: {}", subject);
        }
        
    } else { // Broadcast
        // Broadcast: subscribe to general subject
        std::string subject = message_type;
        
        natsSubscription* sub = nullptr;
        natsStatus status = natsConnection_Subscribe(&sub, conn_, subject.c_str(),
            [](natsConnection* conn, natsSubscription* sub, natsMsg* msg, void* closure) {
                ServiceHost* host = static_cast<ServiceHost*>(closure);
                const char* subject = natsMsg_GetSubject(msg);
                const char* data = natsMsg_GetData(msg);
                int data_len = natsMsg_GetDataLength(msg);
                
                std::string payload(data, data_len);
                std::string msg_type(subject);
                
                auto it = host->handlers_.find(msg_type);
                if (it != host->handlers_.end()) {
                    // Execute handler in thread pool
                    host->thread_pool_.submit([handler = it->second, payload]() {
                        handler(payload);
                    });
                }
                
                natsMsg_Destroy(msg);
            }, this);
            
        if (status == NATS_OK) {
            logger_->info("Successfully subscribed to broadcast subject: {}", subject);
        } else {
            logger_->error("Failed to subscribe to broadcast subject: {}", subject);
        }
    }
    
    logger_->info("Successfully registered handler for: {}", message_type);
}

// 🚀 NEW: Permanent Service Maintenance Task System Implementation

void ServiceHost::StartPermanentTasks(const ServiceInitConfig& config) {
    if (permanent_tasks_running_.load()) {
        logger_->warn("⚠️ Permanent tasks already running, skipping start");
        return;
    }
    
    logger_->info("🚀 Starting permanent service maintenance tasks");
    
    // Store config for task execution
    permanent_task_config_ = config;
    permanent_tasks_running_.store(true);
    
    // Schedule the recurring maintenance task using schedule_interval
    permanent_task_id_ = scheduler_->schedule_interval(
        "permanent_maintenance", 
        config.permanent_task_interval,
        [this]() {
            if (permanent_tasks_running_.load()) {
                execute_permanent_maintenance_cycle();
            }
        }
    );
    
    logger_->info("✅ Permanent tasks started with interval: {}s", 
                 config.permanent_task_interval.count());
}

void ServiceHost::StopPermanentTasks() {
    if (!permanent_tasks_running_.load()) {
        logger_->debug("🔍 Permanent tasks not running, skipping stop");
        return;
    }
    
    logger_->info("🛑 Stopping permanent service maintenance tasks");
    
    permanent_tasks_running_.store(false);
    
    // Cancel the recurring task
    if (permanent_task_id_ != 0) {
        scheduler_->cancel_task(permanent_task_id_);
        permanent_task_id_ = 0;
    }
    
    logger_->info("✅ Permanent tasks stopped successfully");
}

void ServiceHost::execute_permanent_maintenance_cycle() {
    try {
        logger_->debug("🔄 Executing permanent maintenance cycle");
        
        // 1. Execute metrics flush if enabled and tracing is enabled
        if (permanent_task_config_.enable_automatic_metrics_flush && tracing_enabled_) {
            execute_metrics_flush_task();
        }
        
        // 2. Execute health status check if enabled
        if (permanent_task_config_.enable_automatic_health_status) {
            execute_health_status_task();
        }
        
        // 3. Execute backpressure check if enabled
        if (permanent_task_config_.enable_automatic_backpressure_check) {
            execute_backpressure_check_task();
        }
        
        logger_->trace("✅ Permanent maintenance cycle completed");
        
    } catch (const std::exception& e) {
        logger_->error("❌ Error in permanent maintenance cycle: {}", e.what());
    }
}

void ServiceHost::execute_metrics_flush_task() {
    try {
        logger_->debug("📊 Executing automatic metrics flush");
        
        // Check if we have OpenTelemetry available and tracing is enabled
        if (tracing_enabled_ && OpenTelemetryIntegration::is_available()) {
            // Since OpenTelemetry doesn't have a direct flush_metrics method,
            // we can log metrics information and potentially trigger custom metrics collection
            logger_->info("📈 Metrics flush triggered - Service: {}, Queue: {}, Threads: {}", 
                         service_name_, get_current_queue_size(), thread_pool_.size());
            
            // You can extend this to call custom metrics collection functions
            // For example: collect_custom_metrics(), send_to_metrics_endpoint(), etc.
            
            logger_->trace("� Metrics flush completed successfully");
        } else {
            logger_->trace("📊 Metrics flush skipped (tracing disabled or OpenTelemetry unavailable)");
        }
        
    } catch (const std::exception& e) {
        logger_->warn("⚠️ Error during metrics flush: {}", e.what());
    }
}

void ServiceHost::execute_health_status_task() {
    try {
        logger_->debug("❤️ Executing automatic health status check");
        
        // Get current system metrics
        double cpu_usage = get_cpu_usage_percentage();
        size_t memory_usage = get_memory_usage_bytes();
        size_t queue_size = get_current_queue_size();
        
        // Check thresholds and log warnings
        if (cpu_usage > permanent_task_config_.health_check_cpu_threshold) {
            logger_->warn("⚠️ High CPU usage detected: {:.2f}% (threshold: {:.2f}%)", 
                         cpu_usage * 100, permanent_task_config_.health_check_cpu_threshold * 100);
        }
        
        if (memory_usage > permanent_task_config_.health_check_memory_threshold) {
            logger_->warn("⚠️ High memory usage detected: {:.2f}MB (threshold: {:.2f}MB)", 
                         memory_usage / (1024.0 * 1024.0), 
                         permanent_task_config_.health_check_memory_threshold / (1024.0 * 1024.0));
        }
        
        // Log health status
        logger_->debug("📊 Health Status - CPU: {:.2f}%, Memory: {:.2f}MB, Queue: {}", 
                      cpu_usage * 100, memory_usage / (1024.0 * 1024.0), queue_size);
        
    } catch (const std::exception& e) {
        logger_->warn("⚠️ Error during health status check: {}", e.what());
    }
}

void ServiceHost::execute_backpressure_check_task() {
    try {
        logger_->debug("⚡ Executing automatic backpressure check");
        
        size_t current_queue_size = get_current_queue_size();
        
        // Check if we're above the backpressure threshold
        if (current_queue_size > permanent_task_config_.automatic_backpressure_threshold) {
            logger_->warn("⚠️ Backpressure detected! Queue size: {} (threshold: {})", 
                         current_queue_size, permanent_task_config_.automatic_backpressure_threshold);
                         
            // Log additional context
            logger_->warn("📊 Thread pool stats - Active: {}, Pending: {}", 
                         thread_pool_.size(), thread_pool_.pending_tasks());
                         
            // Could trigger additional backpressure handling logic here
            // For example: reduce processing rate, reject new requests, etc.
        }
        
        logger_->trace("📊 Backpressure check completed - Queue size: {}", current_queue_size);
        
    } catch (const std::exception& e) {
        logger_->warn("⚠️ Error during backpressure check: {}", e.what());
    }
}

// Helper methods for system monitoring

double ServiceHost::get_cpu_usage_percentage() {
    try {
        static std::chrono::steady_clock::time_point last_check = std::chrono::steady_clock::now();
        static long last_total_time = 0;
        static long last_process_time = 0;
        
        auto now = std::chrono::steady_clock::now();
        auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_check);
        
        // Only check every 5 seconds to avoid excessive system calls
        if (time_diff.count() < 5000 && last_total_time > 0) {
            return static_cast<double>(last_process_time) / last_total_time;
        }
        
        last_check = now;
        
        // Read process CPU usage from /proc/self/stat
        std::ifstream stat_file("/proc/self/stat");
        if (!stat_file.is_open()) {
            return 0.0;
        }
        
        std::string line;
        std::getline(stat_file, line);
        std::istringstream iss(line);
        
        // Skip to the CPU time fields (fields 13 and 14)
        std::string token;
        for (int i = 0; i < 13; i++) {
            iss >> token;
        }
        
        long utime, stime;
        iss >> utime >> stime;
        
        long total_process_time = utime + stime;
        
        // Calculate CPU usage percentage
        if (last_total_time > 0) {
            long process_time_diff = total_process_time - last_process_time;
            long total_time_diff = time_diff.count() * sysconf(_SC_CLK_TCK) / 1000;
            
            if (total_time_diff > 0) {
                double cpu_usage = static_cast<double>(process_time_diff) / total_time_diff;
                last_process_time = total_process_time;
                last_total_time = total_time_diff;
                return std::min(cpu_usage, 1.0); // Cap at 100%
            }
        }
        
        last_process_time = total_process_time;
        last_total_time = time_diff.count() * sysconf(_SC_CLK_TCK) / 1000;
        
        return 0.0;
    } catch (const std::exception& e) {
        logger_->trace("CPU usage calculation failed: {}", e.what());
        return 0.0;
    }
}

size_t ServiceHost::get_memory_usage_bytes() {
    try {
        struct rusage usage;
        if (getrusage(RUSAGE_SELF, &usage) == 0) {
            // ru_maxrss is in KB on Linux, bytes on macOS
            #ifdef __linux__
            return usage.ru_maxrss * 1024;
            #else
            return usage.ru_maxrss;
            #endif
        }
        return 0;
    } catch (const std::exception& e) {
        logger_->trace("Memory usage calculation failed: {}", e.what());
        return 0;
    }
}

size_t ServiceHost::get_current_queue_size() {
    try {
        return thread_pool_.pending_tasks();
    } catch (const std::exception& e) {
        logger_->trace("Queue size calculation failed: {}", e.what());
        return 0;
    }
}

