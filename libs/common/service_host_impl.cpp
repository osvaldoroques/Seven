#include "service_host.hpp"
#include "messages.pb.h"

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

void ServiceHost::publish_broadcast(const google::protobuf::Message& message) {
    std::lock_guard<std::mutex> lk(publish_mutex_);
    std::string data;
    message.SerializeToString(&data);
    std::string subject = "system.broadcast." + message.GetDescriptor()->full_name();
    status_ = natsConnection_Publish(conn_, subject.c_str(), data.data(), data.size());
    if (status_ != NATS_OK) {
        std::cerr << "❌ Publish broadcast failed: "
                  << natsStatus_GetText(status_) << std::endl;
    }
}


void ServiceHost::publish_point_to_point(const std::string& target_uid,
                                         const google::protobuf::Message& message)
{
    std::lock_guard<std::mutex> lk(publish_mutex_);
    std::string data;
    message.SerializeToString(&data);
    std::string subject = "system.direct." + target_uid + "." + message.GetDescriptor()->full_name();
    status_ = natsConnection_Publish(conn_, subject.c_str(), data.data(), data.size());
    if (status_ != NATS_OK) {
        std::cerr << "❌ Publish P2P failed: "
                  << natsStatus_GetText(status_) << std::endl;
    }
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

// Explicit template instantiation
template void ServiceHost::register_message<Trevor::HealthCheckRequest>(
    MessageRouting routing,
    std::function<void(const Trevor::HealthCheckRequest&)> handler);

template void ServiceHost::register_message<Trevor::HealthCheckResponse>(
    MessageRouting routing,
    std::function<void(const Trevor::HealthCheckResponse&)> handler);

