#pragma once
#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include <iostream>

/**
 * Simplified Configuration class that doesn't depend on yaml-cpp
 * This is a placeholder for testing compilation without external deps
 */
class Configuration {
public:
    explicit Configuration(const std::string& yaml_path)
      : yaml_path_(yaml_path), stop_watch_(false)
    {
        std::cout << "Configuration initialized with file: " << yaml_path << std::endl;
    }

    ~Configuration() {
        stopWatch();
    }

    // Get a value by key - simplified version returns hardcoded defaults
    template<typename T>
    T get(const std::string& key, const T& default_value = T{}) const {
        std::cout << "Getting config key: " << key << std::endl;
        
        // Provide some sensible defaults for our service
        if constexpr (std::is_same_v<T, std::string>) {
            if (key == "nats.url") return "nats://localhost:4222";
            if (key == "service.name") return "portfolio-service";
            return default_value;
        } else if constexpr (std::is_same_v<T, int>) {
            if (key == "threads") return 4;
            if (key == "nats.timeout") return 5000;
            return default_value;
        }
        
        return default_value;
    }

    // Set a callback for configuration reload
    void onReload(std::function<void()> callback) {
        reload_callback_ = callback;
    }

    // Start watching for file changes (simplified - does nothing)
    void startWatch() {
        std::cout << "Starting config file watch (simplified mode)" << std::endl;
        stop_watch_.store(false);
    }

    // Stop watching for file changes
    void stopWatch() {
        if (stop_watch_.exchange(true) == false) {
            std::cout << "Stopping config file watch" << std::endl;
        }
    }

    // Operator bool for conditional checks
    explicit operator bool() const {
        return true; // Always valid for simplified config
    }

    // Reload configuration manually
    void reload() {
        std::cout << "Reloading configuration from: " << yaml_path_ << std::endl;
        if (reload_callback_) {
            reload_callback_();
        }
    }

private:
    std::string yaml_path_;
    std::atomic<bool> stop_watch_;
    std::function<void()> reload_callback_;
};
