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

#include <nats/nats.h>
#include <google/protobuf/message.h>

#include "thread_pool.hpp"

#ifdef HAVE_YAML_CPP
#include "configuration.hpp"
#else
#include "configuration_simple.hpp"
#endif

enum class MessageRouting {
    Broadcast,
    PointToPoint
};

class ServiceHost {
public:
     template<typename... Regs>
    ServiceHost(const std::string& uid,
                const std::string& service_name,
                Regs&&... regs)
      : uid_(uid), service_name_(service_name),
        config_("config.yaml"),
        thread_pool_(config_.get<size_t>("threads", std::thread::hardware_concurrency()))
    {
        // Fold-expression: call Register on each
        (std::forward<Regs>(regs).Register(this), ...);
        
        // Start configuration file watching
        config_.startWatch();
        config_.onReload([this]() {
            std::cout << "🔄 Configuration reloaded" << std::endl;
        });
        
        // Setup signal handlers for graceful shutdown
        setup_signal_handlers();
        
        std::cout << "✅ ServiceHost initialized with " 
                  << config_.get<size_t>("threads", std::thread::hardware_concurrency())
                  << " worker threads" << std::endl;
    }

    // Constructor with custom thread pool size
    template<typename... Regs>
    ServiceHost(const std::string& uid,
                const std::string& service_name,
                size_t thread_pool_size,
                Regs&&... regs)
      : uid_(uid), service_name_(service_name),
        config_("config.yaml"),
        thread_pool_(thread_pool_size)
    {
        // Fold-expression: call Register on each
        (std::forward<Regs>(regs).Register(this), ...);
        
        config_.startWatch();
        config_.onReload([this]() {
            std::cout << "🔄 Configuration reloaded" << std::endl;
        });
        
        // Setup signal handlers for graceful shutdown
        setup_signal_handlers();
        
        std::cout << "✅ ServiceHost initialized with " << thread_pool_size 
                  << " worker threads" << std::endl;
    }

    // Constructor with custom config file
    template<typename... Regs>
    ServiceHost(const std::string& uid,
                const std::string& service_name,
                const std::string& config_file,
                Regs&&... regs)
      : uid_(uid), service_name_(service_name),
        config_(config_file),
        thread_pool_(config_.get<size_t>("threads", std::thread::hardware_concurrency()))
    {
        // Fold-expression: call Register on each
        (std::forward<Regs>(regs).Register(this), ...);
        
        config_.startWatch();
        config_.onReload([this]() {
            std::cout << "🔄 Configuration reloaded" << std::endl;
        });
        
        // Setup signal handlers for graceful shutdown
        setup_signal_handlers();
        
        std::cout << "✅ ServiceHost initialized with config from " << config_file << std::endl;
    }
    virtual ~ServiceHost();

    const std::string& get_uid() const { return uid_; }
    const std::string& get_service_name() const { return service_name_; }
    
    // Graceful shutdown functionality
    void shutdown();
    void setup_signal_handlers();
    bool is_running() const { return running_; }
    void stop() { running_ = false; }
    
    // Graceful shutdown with timeout
    void shutdown_with_timeout(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));
    
    // Configuration access
    template<typename T>
    T get_config(const std::string& key, T default_value) const {
        return config_.get<T>(key, default_value);
    }
    
    // Thread pool utilities
    void submit_task(std::function<void()> task) {
        thread_pool_.submit(std::move(task));
    }
    
    // Health check utilities
    bool is_healthy() const { 
        return running_ && conn_ != nullptr; 
    }
    
    std::string get_status() const {
        if (!running_) return "shutting_down";
        if (!conn_) return "disconnected";
        return "healthy";
    }

     // Register a handler for one message type T
    template<typename T>
    void register_message(MessageRouting routing,
                          std::function<void(const T&)> handler)
    {
        const std::string type_name = T::descriptor()->full_name();
        handlers_[type_name] = [handler, type_name](const std::string& raw) {
            T msg;
            if (!msg.ParseFromString(raw)) {
                std::cerr << "❌ Failed to parse " << type_name << std::endl;
                return;
            }
            handler(msg);
        };

        if (routing == MessageRouting::Broadcast) {
            if (conn_) {
                subscribe_broadcast(type_name);
            }
        } else {
            if (conn_) {
                subscribe_point_to_point(type_name);
            }
        }

        std::cout << "✅ Registered handler for: " << type_name << std::endl;
    }

   // Dispatch incoming raw payload to the correct handler
    void receive_message(const std::string& type_name,
                         const std::string& payload)
    {
        auto it = handlers_.find(type_name);
        if (it != handlers_.end()) {
            // Offload to thread pool for parallel processing
            thread_pool_.submit([handler = it->second, payload, type_name]() {
                std::cout << "🔄 Processing " << type_name << " in worker thread " 
                          << std::this_thread::get_id() << std::endl;
                handler(payload);
            });
        } else {
            std::cerr << "⚠️ No handler registered for message type: "
                      << type_name << std::endl;
        }
    }


    void init_nats(const std::string& nats_url = "nats://localhost:4222");
    void init_jetstream();

    void publish_broadcast(const google::protobuf::Message& message);
    void publish_point_to_point(const std::string& target_uid, const google::protobuf::Message& message);

   

private:

    void subscribe_broadcast(const std::string& type_name);
    void subscribe_point_to_point(const std::string& type_name);

    std::string uid_;
    std::string service_name_;

    natsConnection* conn_ = nullptr;
    jsCtx* js_ = nullptr;
    natsStatus status_;

    using HandlerFunc = std::function<void(const std::string&)>;
    std::unordered_map<std::string, HandlerFunc> handlers_;

    Configuration config_;  // Configuration for service settings
    ThreadPool thread_pool_;  // Thread pool for parallel message processing
    std::mutex publish_mutex_;  // Ensure thread-safe publishing
    
    std::atomic<bool> running_{true};  // Flag for graceful shutdown
    
public:
    static ServiceHost* instance_;  // For signal handler access (public)
    
private:
};
