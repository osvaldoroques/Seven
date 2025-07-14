#pragma once

#include <iostream>
#include <string>
#include <functional>
#include <unordered_map>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <csignal>
#include <chrono>
#include <sstream>

#include <nats/nats.h>
#include <google/protobuf/message.h>

#include "thread_pool.hpp"
#include "logger.hpp"
#include "opentelemetry_integration.hpp"

#ifdef HAVE_YAML_CPP
#include "configuration.hpp"
#else
#include "configuration_simple.hpp"
#endif

enum class MessageRouting
{
    Broadcast,
    PointToPoint
};

class ServiceHost
{
public:
    template <typename... Regs>
    ServiceHost(const std::string &uid,
                const std::string &service_name,
                Regs &&...regs)
        : uid_(uid), service_name_(service_name),
          config_("config.yaml"),
          thread_pool_(config_.get<size_t>("threads", std::thread::hardware_concurrency())),
          logger_(std::make_shared<Logger>(service_name_, uid)),
          tracing_enabled_(false),
          publish_broadcast_impl_(&ServiceHost::publish_broadcast_fast),
          publish_point_to_point_impl_(&ServiceHost::publish_point_to_point_fast)
    {
        // Initialize logging system
        Logger::set_level_from_env();
        Logger::setup_signal_handler();

#ifdef HAVE_SPDLOG
        logger_->info("Initializing ServiceHost - UID: {}, Service: {}", uid_, service_name_);
#else
        logger_->info("Initializing ServiceHost - UID: " + uid_ + ", Service: " + service_name_);
#endif

        // Initialize OpenTelemetry
        const char *otlp_endpoint = std::getenv("OTEL_EXPORTER_OTLP_ENDPOINT");
        if (otlp_endpoint)
        {
            OpenTelemetryIntegration::initialize(service_name_, otlp_endpoint);
            logger_->info("OpenTelemetry initialized with endpoint: {}", otlp_endpoint);
        }
        else
        {
            logger_->warn("OTEL_EXPORTER_OTLP_ENDPOINT not set, skipping OpenTelemetry initialization");
        }

        // Fold-expression: call Register on each
        (std::forward<Regs>(regs).Register(this), ...);

        // Start configuration file watching
        config_.startWatch();
        config_.onReload([this]()
                         { logger_->info("Configuration reloaded"); });

        // Setup signal handlers for graceful shutdown
        setup_signal_handlers();

        logger_->info("ServiceHost initialized with {} worker threads",
                      config_.get<size_t>("threads", std::thread::hardware_concurrency()));
    }

    // Constructor with custom thread pool size
    template <typename... Regs>
    ServiceHost(const std::string &uid,
                const std::string &service_name,
                size_t thread_pool_size,
                Regs &&...regs)
        : uid_(uid), service_name_(service_name),
          config_("config.yaml"),
          thread_pool_(thread_pool_size),
          tracing_enabled_(false),
          publish_broadcast_impl_(&ServiceHost::publish_broadcast_fast),
          publish_point_to_point_impl_(&ServiceHost::publish_point_to_point_fast)
    {
        // Fold-expression: call Register on each
        (std::forward<Regs>(regs).Register(this), ...);

        config_.startWatch();
        config_.onReload([this]()
                         { std::cout << "🔄 Configuration reloaded" << std::endl; });

        // Setup signal handlers for graceful shutdown
        setup_signal_handlers();

        std::cout << "✅ ServiceHost initialized with " << thread_pool_size
                  << " worker threads" << std::endl;
    }

    // Constructor with custom config file
    template <typename... Regs>
    ServiceHost(const std::string &uid,
                const std::string &service_name,
                const std::string &config_file,
                Regs &&...regs)
        : uid_(uid), service_name_(service_name),
          config_(config_file),
          thread_pool_(config_.get<size_t>("threads", std::thread::hardware_concurrency())),
          logger_(std::make_shared<Logger>(service_name_, uid)),
          tracing_enabled_(false),
          publish_broadcast_impl_(&ServiceHost::publish_broadcast_fast),
          publish_point_to_point_impl_(&ServiceHost::publish_point_to_point_fast)
    {
        // Initialize logging system
        Logger::set_level_from_env();
        Logger::setup_signal_handler();

        logger_->info("Initializing ServiceHost with custom config - UID: {}, Service: {}, Config: {}",
                      uid_, service_name_, config_file);

        // Fold-expression: call Register on each
        (std::forward<Regs>(regs).Register(this), ...);

        config_.startWatch();
        config_.onReload([this]()
                         { logger_->info("Configuration reloaded"); });

        // Setup signal handlers for graceful shutdown
        setup_signal_handlers();

        logger_->info("ServiceHost initialized with config from {}", config_file);
    }
    virtual ~ServiceHost();

    const std::string &get_uid() const { return uid_; }
    const std::string &get_service_name() const { return service_name_; }

    // Logger access for message handlers
    std::shared_ptr<Logger> get_logger() const { return logger_; }
    std::shared_ptr<Logger> create_request_logger() const
    {
        return logger_->create_request_logger();
    }

    // Graceful shutdown functionality
    void shutdown();
    void setup_signal_handlers();
    bool is_running() const { return running_; }
    void stop() { running_ = false; }

    // Graceful shutdown with timeout
    void shutdown_with_timeout(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

    // Configuration access
    template <typename T>
    T get_config(const std::string &key, T default_value) const
    {
        return config_.get<T>(key, default_value);
    }

    // Thread pool utilities
    void submit_task(std::function<void()> task)
    {
        thread_pool_.submit(std::move(task));
    }

    // Health check utilities
    bool is_healthy() const
    {
        return running_ && conn_ != nullptr;
    }

    std::string get_status() const
    {
        if (!running_)
            return "shutting_down";
        if (!conn_)
            return "disconnected";
        return "healthy";
    }

    // Register a handler for one message type T
    template <typename T>
    void register_message(MessageRouting routing,
                          std::function<void(const T &)> handler)
    {
        const std::string type_name = T::descriptor()->full_name();

        logger_->info("Registering handler for message type: {}, routing: {}",
                      type_name,
                      routing == MessageRouting::Broadcast ? "Broadcast" : "PointToPoint");

        handlers_[type_name] = [this, handler, type_name](const std::string &raw)
        {
            auto request_logger = create_request_logger();
            request_logger->debug("Processing message: {}, size: {} bytes", type_name, raw.size());

            T msg;
            if (!msg.ParseFromString(raw))
            {
                request_logger->error("Failed to parse message: {}", type_name);
                return;
            }

            auto start_time = std::chrono::high_resolution_clock::now();

            // Submit to thread pool with logging
            thread_pool_.submit([handler, msg, request_logger, type_name, start_time]()
                                {
                request_logger->trace("Handler execution started for: {}", type_name);
                
                try {
                    handler(msg);
                    
                    auto end_time = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                        end_time - start_time).count();
                    
                    request_logger->debug("Handler completed for: {}, duration: {}μs", 
                                        type_name, duration);
                } catch (const std::exception& e) {
                    request_logger->error("Handler failed for: {}, error: {}", type_name, e.what());
                } catch (...) {
                    request_logger->error("Handler failed for: {} with unknown exception", type_name);
                } });
        };

        if (routing == MessageRouting::Broadcast)
        {
            if (conn_)
            {
                #ifdef HAVE_OPENTELEMETRY
                subscribe_broadcast_V2(type_name);  // Use V2 with tracing
                #else
                subscribe_broadcast(type_name);     // Fallback to V1
                #endif
            }
        }
        else
        {
            if (conn_)
            {
                #ifdef HAVE_OPENTELEMETRY
                subscribe_point_to_point_V2(type_name);  // Use V2 with tracing
                #else
                subscribe_point_to_point(type_name);     // Fallback to V1
                #endif
            }
        }

        logger_->info("Successfully registered handler for: {}", type_name);
    }

    // Dispatch incoming raw payload to the correct handler with tracing
    void receive_message(const std::string &type_name,
                         const std::string &payload)
    {
        auto it = handlers_.find(type_name);
        if (it != handlers_.end())
        {
            // Offload to thread pool for parallel processing with tracing
            thread_pool_.submit([handler = it->second, payload, type_name, this]()
                                {
                // Start receive span
                TRACE_SPAN("ServiceHost::receive_message");
                _trace_span.add_attributes({
                    {"messaging.operation", "receive"},
                    {"messaging.destination", type_name},
                    {"service.name", service_name_},
                    {"service.instance.id", uid_}
                });

                auto [trace_id, span_id] = _trace_span.get_trace_and_span_ids();
                std::ostringstream thread_id_stream;
                thread_id_stream << std::this_thread::get_id();
                logger_->debug("Processing {} in worker thread {} trace_id={} span_id={}", type_name, thread_id_stream.str(), trace_id, span_id);
                handler(payload);
            });
        }
        else
        {
            logger_->warn("No handler registered for message type: {}", type_name);
        }
    }

    // Helper to extract trace context from protobuf message
    template <typename T>
    std::unordered_map<std::string, std::string> extract_trace_context_from_message(const T &message)
    {
        std::unordered_map<std::string, std::string> context;
        if (message.has_trace_metadata())
        {
            const auto &metadata = message.trace_metadata();
            if (!metadata.traceparent().empty())
            {
                context["traceparent"] = metadata.traceparent();
            }
            if (!metadata.tracestate().empty())
            {
                context["tracestate"] = metadata.tracestate();
            }
        }
        return context;
    }

    // Helper to inject trace context into protobuf message
    template <typename T>
    void inject_trace_context_into_message(T &message, std::shared_ptr<void> span = nullptr)
    {
        auto headers = OpenTelemetryIntegration::inject_trace_context(span);
        if (!headers.empty())
        {
            auto *metadata = message.mutable_trace_metadata();
            auto it = headers.find("traceparent");
            if (it != headers.end())
            {
                metadata->set_traceparent(it->second);
            }
            it = headers.find("tracestate");
            if (it != headers.end())
            {
                metadata->set_tracestate(it->second);
            }
            // Also set correlation ID from logger
            metadata->set_correlation_id(logger_->get_correlation_id());
        }
    }    void init_nats(const std::string &nats_url = "nats://localhost:4222");
    void init_jetstream();

    // 🚀 Hot-path methods with function pointer optimization
    void publish_broadcast(const google::protobuf::Message &message);
    void publish_point_to_point(const std::string &target_uid, const google::protobuf::Message &message);
    
    // Dynamic tracing control
    void enable_tracing();
    void disable_tracing();
    bool is_tracing_enabled() const { return tracing_enabled_; }

    // Legacy V2 methods (kept for compatibility)
    void publish_broadcast_V2(const google::protobuf::Message &message);
    void publish_point_to_point_V2(const std::string &target_uid, const google::protobuf::Message &message);

private:
    void subscribe_broadcast(const std::string &type_name);
    void subscribe_point_to_point(const std::string &type_name);
    
    // V2 methods with OpenTelemetry trace context support
    void subscribe_broadcast_V2(const std::string &type_name);
    void subscribe_point_to_point_V2(const std::string &type_name);

    std::string uid_;
    std::string service_name_;

    natsConnection *conn_ = nullptr;
    jsCtx *js_ = nullptr;
    natsStatus status_;

    using HandlerFunc = std::function<void(const std::string &)>;
    std::unordered_map<std::string, HandlerFunc> handlers_;

    Configuration config_;           // Configuration for service settings
    ThreadPool thread_pool_;         // Thread pool for parallel message processing
    std::shared_ptr<Logger> logger_; // Structured logging with correlation IDs
    std::mutex publish_mutex_;       // Ensure thread-safe publishing

    std::atomic<bool> running_{true}; // Flag for graceful shutdown
    
    // 🚀 Function pointer optimization for hot-path methods (zero-branching)
    using PublishBroadcastFunc = void (ServiceHost::*)(const google::protobuf::Message &);
    using PublishP2PFunc = void (ServiceHost::*)(const std::string &, const google::protobuf::Message &);
    
    PublishBroadcastFunc publish_broadcast_impl_;
    PublishP2PFunc publish_point_to_point_impl_;
    bool tracing_enabled_;
    
    // Non-traced implementations (zero overhead)
    void publish_broadcast_fast(const google::protobuf::Message &message);
    void publish_point_to_point_fast(const std::string &target_uid, const google::protobuf::Message &message);
    
    // Traced implementations (OpenTelemetry enabled)
    void publish_broadcast_traced(const google::protobuf::Message &message);
    void publish_point_to_point_traced(const std::string &target_uid, const google::protobuf::Message &message);

public:
    static ServiceHost *instance_; // For signal handler access (public)

private:
};
