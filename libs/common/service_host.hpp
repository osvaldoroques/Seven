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
#include <future>         // For std::future, std::promise, std::async

#include <nats/nats.h>
#include <google/protobuf/message.h>

#include "thread_pool.hpp"
#include "logger.hpp"
#include "opentelemetry_integration.hpp"
#include "configuration.hpp"
#include "service_cache.hpp"
#include "service_scheduler.hpp"

// Forward declaration
class ServiceCache;
class ServiceScheduler;

// Service initialization configuration
struct ServiceInitConfig {
    // NATS Configuration
    std::string nats_url = "nats://localhost:4222";
    bool enable_jetstream = true;
    
    // Cache Configuration
    bool enable_cache = true;
    size_t default_cache_size = 1000;
    std::chrono::seconds default_cache_ttl = std::chrono::hours(1);
    
    // Scheduler Configuration
    bool enable_scheduler = true;
    bool enable_auto_cache_cleanup = true;
    std::chrono::minutes cache_cleanup_interval = std::chrono::minutes(5);
    
    // Monitoring & Metrics
    bool enable_metrics_flush = false;
    std::chrono::seconds metrics_flush_interval = std::chrono::seconds(30);
    std::function<void()> metrics_flush_callback = nullptr;
    
    // Health Check Configuration
    bool enable_health_heartbeat = false;
    std::chrono::seconds health_heartbeat_interval = std::chrono::seconds(10);
    std::function<void()> health_heartbeat_callback = nullptr;
    
    // Back-pressure Monitoring
    bool enable_backpressure_monitor = false;
    size_t backpressure_threshold = 100;
    std::function<size_t()> queue_size_func = nullptr;
    std::function<void()> backpressure_callback = nullptr;
    
    // Performance Configuration
    bool enable_performance_mode = false;  // If true, starts with tracing disabled
    
    // OpenTelemetry Configuration
    bool force_otel_initialization = false;
    std::string custom_otel_endpoint = "";
    
    // 🚀 NEW: Permanent Service Maintenance Tasks
    bool enable_permanent_tasks = true;  // Enable automatic service maintenance
    std::chrono::seconds permanent_task_interval = std::chrono::seconds(30);  // How often to run maintenance
    
    // Individual task controls
    bool enable_automatic_metrics_flush = true;   // Auto flush metrics when tracing enabled
    bool enable_automatic_health_status = true;   // Auto send health status
    bool enable_automatic_backpressure_check = true;  // Auto check for backpressure
    
    // Thresholds and limits
    size_t automatic_backpressure_threshold = 100;  // Queue size threshold for backpressure
    double health_check_cpu_threshold = 0.8;        // CPU threshold for health warnings
    size_t health_check_memory_threshold = 1024 * 1024 * 1024;  // 1GB memory threshold
};

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
          publish_point_to_point_impl_(&ServiceHost::publish_point_to_point_fast),
          cache_(std::make_unique<ServiceCache>(this)),
          scheduler_(std::make_unique<ServiceScheduler>(&thread_pool_, logger_))
    {
        // Initialize logging system (safe, minimal)
        Logger::set_level_from_env();
        Logger::setup_signal_handler();

        logger_->info("ServiceHost constructor - UID: {}, Service: {}", uid_, service_name_);

        // Fold-expression: call Register on each (safe, just registration)
        (std::forward<Regs>(regs).Register(this), ...);

        logger_->info("ServiceHost constructor completed - {} worker threads configured",
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
          publish_point_to_point_impl_(&ServiceHost::publish_point_to_point_fast),
          cache_(std::make_unique<ServiceCache>(this)),
          scheduler_(std::make_unique<ServiceScheduler>(&thread_pool_, logger_))
    {
        // Initialize logging system (safe, minimal)
        Logger::set_level_from_env();
        Logger::setup_signal_handler();

        logger_->info("ServiceHost constructor - UID: {}, Service: {}, Threads: {}", 
                      uid_, service_name_, thread_pool_size);

        // Fold-expression: call Register on each (safe, just registration)
        (std::forward<Regs>(regs).Register(this), ...);

        logger_->info("ServiceHost constructor completed with {} worker threads", thread_pool_size);
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
          publish_point_to_point_impl_(&ServiceHost::publish_point_to_point_fast),
          cache_(std::make_unique<ServiceCache>(this)),
          scheduler_(std::make_unique<ServiceScheduler>(&thread_pool_, logger_))
    {
        // Initialize logging system (safe, minimal)
        Logger::set_level_from_env();
        Logger::setup_signal_handler();

        logger_->info("ServiceHost constructor - UID: {}, Service: {}, Config: {}", 
                      uid_, service_name_, config_file);

        // Fold-expression: call Register on each (safe, just registration)
        (std::forward<Regs>(regs).Register(this), ...);

        logger_->info("ServiceHost constructor completed with config from {}", config_file);
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

    // Thread pool access
    ThreadPool& get_thread_pool() { return thread_pool_; }
    const ThreadPool& get_thread_pool() const { return thread_pool_; }

    // 🚀 NEW: Simplified Handler Registration System
    // Handler takes raw payload; your logic will parse it
    using HandlerRaw = std::function<void(const std::string& payload)>;

    // Map: message type name → (routing, handler)
    using RegistrationMap = std::unordered_map<std::string, std::pair<MessageRouting, HandlerRaw>>;

    // Batch-register handlers from a map
    void register_handlers(const RegistrationMap& regs);

    // Individual handler registration (alternative to map-based)
    void register_handler(const std::string& message_type, 
                         MessageRouting routing, 
                         HandlerRaw handler);

protected:
    // Protected access for derived classes
    natsConnection* get_nats_connection() { return conn_; }
    
public:
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

    // Cache access - Core feature for all services
    ServiceCache& get_cache() { return *cache_; }
    const ServiceCache& get_cache() const { return *cache_; }
    
    // Convenience methods for common cache operations
    template<typename Key, typename Value>
    auto create_cache(const std::string& name, size_t max_size, 
                     std::chrono::seconds ttl = std::chrono::seconds(0))
    {
        return cache_->template create_cache<Key, Value>(name, max_size, ttl);
    }
    
    template<typename Key, typename Value>
    auto get_cache_instance(const std::string& name)
    {
        return cache_->template get_cache_instance<Key, Value>(name);
    }

    // Scheduler access - Built-in task scheduling for all services
    ServiceScheduler& get_scheduler() { return *scheduler_; }
    const ServiceScheduler& get_scheduler() const { return *scheduler_; }
    
    // Convenience methods for common scheduling patterns
    using TaskId = ServiceScheduler::TaskId;
    
    // Schedule metrics flush every 30 seconds
    TaskId schedule_metrics_flush(std::function<void()> flush_func) {
        return scheduler_->schedule_metrics_flush(std::move(flush_func));
    }
    
    // Schedule cache cleanup every 5 minutes
    TaskId schedule_cache_cleanup(std::function<void()> cleanup_func) {
        return scheduler_->schedule_cache_cleanup(std::move(cleanup_func));
    }
    
    // Schedule health check heartbeat every 10 seconds
    TaskId schedule_health_heartbeat(std::function<void()> heartbeat_func) {
        return scheduler_->schedule_health_heartbeat(std::move(heartbeat_func));
    }
    
    // Schedule back-pressure monitoring
    TaskId schedule_backpressure_monitor(std::function<size_t()> queue_size_func,
                                        size_t threshold,
                                        std::function<void()> alert_func) {
        return scheduler_->schedule_backpressure_monitor(std::move(queue_size_func), 
                                                        threshold, std::move(alert_func));
    }
    
    // General scheduling methods
    TaskId schedule_interval(const std::string& name, 
                           std::chrono::milliseconds interval,
                           std::function<void()> task) {
        return scheduler_->schedule_interval(name, interval, std::move(task));
    }
    
    TaskId schedule_once(const std::string& name,
                        std::chrono::milliseconds delay,
                        std::function<void()> task) {
        return scheduler_->schedule_once(name, delay, std::move(task));
    }

    // 🚀 COMPREHENSIVE SERVICE INITIALIZATION 🚀
    // One-stop initialization for all service functionalities
    void initialize_service(const ServiceInitConfig& config = {});
    
    // 🚀 NEW: StartService - Complete service startup with configuration
    // This method handles all service initialization and startup in one call
    void StartService(const ServiceInitConfig& config = {});
    
    // 🚀 NEW: StartServiceAsync - Non-blocking service startup
    // Returns a future that completes when service infrastructure is ready
    std::future<void> StartServiceAsync(const ServiceInitConfig& config = {});
    
    // 🚀 NEW: StartServiceInfrastructureAsync - Initialize just the infrastructure
    // Returns a future for NATS, JetStream, and core systems initialization
    std::future<void> StartServiceInfrastructureAsync(const ServiceInitConfig& config = {});
    
    // 🚀 NEW: CompleteServiceStartup - Complete startup after async infrastructure init
    // Call this after infrastructure future is ready to finish service setup
    std::future<void> CompleteServiceStartup(const ServiceInitConfig& config = {});
    
    // 🚀 NEW: Permanent Service Maintenance Tasks
    // Start automatic service maintenance tasks (metrics, health, backpressure)
    void StartPermanentTasks(const ServiceInitConfig& config = {});
    
    // Stop permanent service maintenance tasks
    void StopPermanentTasks();
    
    // Check if permanent tasks are running
    bool IsPermanentTasksRunning() const { return permanent_tasks_running_.load(); }
    
    // Start subscription processing for registered message handlers
    void start_subscription_processing();
    
    // Create a default service initialization config
    static ServiceInitConfig create_default_config() {
        return ServiceInitConfig{};
    }
    
    // Create a production service config with common settings
    static ServiceInitConfig create_production_config() {
        ServiceInitConfig config;
        config.enable_cache = true;
        config.default_cache_size = 5000;
        config.default_cache_ttl = std::chrono::hours(2);
        config.enable_metrics_flush = true;
        config.enable_health_heartbeat = true;
        config.enable_backpressure_monitor = true;
        config.backpressure_threshold = 200;
        
        // Enable permanent service maintenance tasks
        config.enable_permanent_tasks = true;
        config.permanent_task_interval = std::chrono::seconds(30);
        config.enable_automatic_metrics_flush = true;
        config.enable_automatic_health_status = true;
        config.enable_automatic_backpressure_check = true;
        config.automatic_backpressure_threshold = 200;
        
        return config;
    }
    
    // Create a development service config with enhanced monitoring
    static ServiceInitConfig create_development_config() {
        ServiceInitConfig config;
        config.enable_cache = true;
        config.default_cache_size = 1000;
        config.enable_metrics_flush = true;
        config.enable_health_heartbeat = true;
        config.enable_backpressure_monitor = true;
        config.backpressure_threshold = 50;
        config.enable_performance_mode = false;  // Full tracing in dev
        
        // Enable permanent service maintenance tasks with more frequent checks in dev
        config.enable_permanent_tasks = true;
        config.permanent_task_interval = std::chrono::seconds(15);
        config.enable_automatic_metrics_flush = true;
        config.enable_automatic_health_status = true;
        config.enable_automatic_backpressure_check = true;
        config.automatic_backpressure_threshold = 50;
        
        return config;
    }
    
    // Create a high-performance service config
    static ServiceInitConfig create_performance_config() {
        ServiceInitConfig config;
        config.enable_cache = true;
        config.default_cache_size = 10000;
        config.default_cache_ttl = std::chrono::minutes(30);
        config.enable_performance_mode = true;  // Tracing disabled for speed
        config.enable_metrics_flush = false;    // Minimal overhead
        config.enable_health_heartbeat = false;
        config.enable_backpressure_monitor = true;
        config.backpressure_threshold = 500;
        return config;
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
                subscribe_broadcast_V2(type_name);  // Use V2 with tracing
            }
        }
        else
        {
            if (conn_)
            {
                subscribe_point_to_point_V2(type_name);  // Use V2 with tracing
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
    void init_cache_system();

    // Enable/disable OpenTelemetry tracing (function pointer optimization)
    void enable_tracing();
    void disable_tracing();
    bool is_tracing_enabled() const { return tracing_enabled_; }
    
    // 🚀 Performance benchmarking and validation
    void run_performance_benchmark(int iterations = 10000, bool verbose = true);

    // Optimized publish methods with function pointer dispatch
    void publish_broadcast(const google::protobuf::Message &message);
    void publish_point_to_point(const std::string &target_uid, const google::protobuf::Message &message);

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
    std::unique_ptr<ServiceCache> cache_; // Integrated LRU caching system
    std::unique_ptr<ServiceScheduler> scheduler_; // Integrated task scheduler

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
    
    // 🚀 NEW: Permanent Service Maintenance Task System
    std::atomic<bool> permanent_tasks_running_{false};
    TaskId permanent_task_id_{0};
    ServiceInitConfig permanent_task_config_;
    
    // Permanent task execution methods
    void start_permanent_tasks(const ServiceInitConfig& config);
    void stop_permanent_tasks();
    void execute_permanent_maintenance_cycle();
    
    // Individual maintenance task methods
    void execute_metrics_flush_task();
    void execute_health_status_task();
    void execute_backpressure_check_task();
    
    // Helper methods for maintenance tasks
    double get_cpu_usage_percentage();
    size_t get_memory_usage_bytes();
    size_t get_current_queue_size();

public:
    static ServiceHost *instance_; // For signal handler access (public)

private:
};
