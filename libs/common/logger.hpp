#pragma once

#include <string>
#include <memory>
#include <sstream>
#include <chrono>
#include <random>
#include <iomanip>
#include <iostream>
#include <atomic>
#include <thread>
#include <csignal>
#include <mutex>
#include <cstdlib>

#ifdef HAVE_SPDLOG
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <fmt/format.h>
#endif

/**
 * Structured logging system with correlation IDs and dynamic log levels
 * Supports both spdlog (when available) and simple stdout fallback
 */
class Logger {
public:
    enum class Level {
        TRACE = 0,
        DEBUG = 1,
        INFO = 2,
        WARN = 3,
        ERROR = 4,
        CRITICAL = 5
    };

private:
    std::string service_name_;
    std::string correlation_id_;
    std::string trace_id_;
    std::string span_id_;
    static std::atomic<Level> global_level_;
    static std::shared_ptr<Logger> instance_;
    static std::mutex instance_mutex_;

#ifdef HAVE_SPDLOG
    std::shared_ptr<spdlog::logger> logger_;
#endif

    // Generate correlation ID (8-character hex)
    static std::string generate_correlation_id() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);
        
        std::stringstream ss;
        for (int i = 0; i < 8; ++i) {
            ss << std::hex << dis(gen);
        }
        return ss.str();
    }

    // Generate trace ID (16-character hex - OpenTelemetry compatible)
    static std::string generate_trace_id() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);
        
        std::stringstream ss;
        for (int i = 0; i < 16; ++i) {
            ss << std::hex << dis(gen);
        }
        return ss.str();
    }

    // Generate span ID (8-character hex - OpenTelemetry compatible)
    static std::string generate_span_id() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);
        
        std::stringstream ss;
        for (int i = 0; i < 8; ++i) {
            ss << std::hex << dis(gen);
        }
        return ss.str();
    }

    // Ensure logs directory exists
    static void ensure_logs_directory() {
#ifdef HAVE_SPDLOG
        // Create logs directory if it doesn't exist
        // This is platform-specific, but works on Linux/Unix systems
        std::system("mkdir -p logs");
#endif
    }

    // Convert level to string
    static const char* level_to_string(Level level) {
        switch (level) {
            case Level::TRACE: return "TRACE";
            case Level::DEBUG: return "DEBUG";
            case Level::INFO: return "INFO";
            case Level::WARN: return "WARN";
            case Level::ERROR: return "ERROR";
            case Level::CRITICAL: return "CRITICAL";
        }
        return "UNKNOWN";
    }

#ifdef HAVE_SPDLOG
    // Convert our level to spdlog level
    static spdlog::level::level_enum to_spdlog_level(Level level) {
        switch (level) {
            case Level::TRACE: return spdlog::level::trace;
            case Level::DEBUG: return spdlog::level::debug;
            case Level::INFO: return spdlog::level::info;
            case Level::WARN: return spdlog::level::warn;
            case Level::ERROR: return spdlog::level::err;
            case Level::CRITICAL: return spdlog::level::critical;
        }
        return spdlog::level::info;
    }
#endif

public:
    explicit Logger(const std::string& service_name, const std::string& correlation_id = "", 
                   const std::string& trace_id = "", const std::string& span_id = "")
        : service_name_(service_name), 
          correlation_id_(correlation_id.empty() ? generate_correlation_id() : correlation_id),
          trace_id_(trace_id.empty() ? generate_trace_id() : trace_id),
          span_id_(span_id.empty() ? generate_span_id() : span_id)
    {
#ifdef HAVE_SPDLOG
        // Ensure logs directory exists before creating file sinks
        ensure_logs_directory();
        
        // Create logger with console and daily rotating file sinks
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        
        // Daily file rotation: one file per day with timestamp in filename
        // Files will be named like: service_name_2025-07-12.log
        auto daily_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(
            "logs/" + service_name + "_%Y-%m-%d.log", 
            0,  // rotation hour (midnight)
            0   // rotation minute
        );
        
        // Optional: Also add a rotating file sink as backup for large log volumes
        auto rotating_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            "logs/" + service_name + "_current.log", 
            1024*1024*50,  // 50MB per file
            5              // keep 5 backup files
        );

        logger_ = std::make_shared<spdlog::logger>(
            service_name_, 
            spdlog::sinks_init_list{console_sink, daily_sink, rotating_sink}
        );
        
        // Set pattern: [timestamp] [level] [service] [correlation_id] message
        logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%f] [%l] [%n] [%v]");
        logger_->set_level(to_spdlog_level(global_level_.load()));
        logger_->flush_on(spdlog::level::warn);
        
        spdlog::register_logger(logger_);
#endif
    }

    // Create child logger with same trace context for component tracing
    std::shared_ptr<Logger> create_child(const std::string& component) const {
        return std::make_shared<Logger>(service_name_ + "::" + component, correlation_id_, trace_id_, generate_span_id());
    }

    // Create new correlation ID and trace context for new request/operation
    std::shared_ptr<Logger> create_request_logger() const {
        return std::make_shared<Logger>(service_name_, generate_correlation_id(), generate_trace_id(), generate_span_id());
    }

    // Create span logger with new span ID but same trace
    std::shared_ptr<Logger> create_span_logger(const std::string& operation_name = "") const {
        std::string service = operation_name.empty() ? service_name_ : service_name_ + "::" + operation_name;
        return std::make_shared<Logger>(service, correlation_id_, trace_id_, generate_span_id());
    }

    // Template method for structured logging
    template<typename... Args>
    void log(Level level, const std::string& format, Args&&... args) {
        if (level < global_level_.load()) {
            return; // Skip if below current log level
        }

#ifdef HAVE_SPDLOG
        std::string message;
        if constexpr (sizeof...(args) > 0) {
            message = fmt::format(format, std::forward<Args>(args)...);
        } else {
            message = format;
        }
        
        // Create structured log with correlation_id, trace_id, and span_id
        std::string structured_message = fmt::format(
            "correlation_id={} trace_id={} span_id={} service={} message=\"{}\"",
            correlation_id_, trace_id_, span_id_, service_name_, message
        );
        logger_->log(to_spdlog_level(level), structured_message);
#else
        // Fallback: stdout logging with simple {} replacement
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::string message;
        if constexpr (sizeof...(args) > 0) {
            message = format_fallback(format, std::forward<Args>(args)...);
        } else {
            message = format;
        }

        std::cout << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
                  << "." << std::setfill('0') << std::setw(3) << ms.count() << "] "
                  << "[" << level_to_string(level) << "] "
                  << "correlation_id=" << correlation_id_ << " "
                  << "trace_id=" << trace_id_ << " "
                  << "span_id=" << span_id_ << " "
                  << "service=" << service_name_ << " "
                  << "message=\"" << message << "\"" << std::endl;
#endif
    }

    // Convenience methods
    template<typename... Args>
    void trace(const std::string& format, Args&&... args) {
        log(Level::TRACE, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void debug(const std::string& format, Args&&... args) {
        log(Level::DEBUG, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void info(const std::string& format, Args&&... args) {
        log(Level::INFO, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void warn(const std::string& format, Args&&... args) {
        log(Level::WARN, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void error(const std::string& format, Args&&... args) {
        log(Level::ERROR, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void critical(const std::string& format, Args&&... args) {
        log(Level::CRITICAL, format, std::forward<Args>(args)...);
    }

    // Global log level management
    static void set_level(Level level) {
        global_level_.store(level);
#ifdef HAVE_SPDLOG
        spdlog::set_level(to_spdlog_level(level));
#endif
    }

    static Level get_level() {
        return global_level_.load();
    }

    // Set level from environment variable
    static void set_level_from_env(const char* env_var = "LOG_LEVEL") {
        const char* level_str = std::getenv(env_var);
        if (level_str) {
            std::string level(level_str);
            if (level == "TRACE") set_level(Level::TRACE);
            else if (level == "DEBUG") set_level(Level::DEBUG);
            else if (level == "INFO") set_level(Level::INFO);
            else if (level == "WARN") set_level(Level::WARN);
            else if (level == "ERROR") set_level(Level::ERROR);
            else if (level == "CRITICAL") set_level(Level::CRITICAL);
        }
    }

    // Signal handler for dynamic level changes (SIGHUP)
    static void setup_signal_handler() {
        std::signal(SIGHUP, [](int) {
            set_level_from_env();
            std::cout << "Log level reloaded from environment\n";
        });
    }

    const std::string& get_correlation_id() const { return correlation_id_; }
    const std::string& get_trace_id() const { return trace_id_; }
    const std::string& get_span_id() const { return span_id_; }
    const std::string& get_service_name() const { return service_name_; }

private:
    // Simple fallback formatter for {} replacement when spdlog is not available
    template<typename T>
    std::string format_fallback(const std::string& format, T&& arg) {
        std::string result = format;
        size_t pos = result.find("{}");
        if (pos != std::string::npos) {
            std::ostringstream oss;
            oss << arg;
            result.replace(pos, 2, oss.str());
        }
        return result;
    }

    template<typename T, typename... Args>
    std::string format_fallback(const std::string& format, T&& arg, Args&&... args) {
        std::string result = format;
        size_t pos = result.find("{}");
        if (pos != std::string::npos) {
            std::ostringstream oss;
            oss << arg;
            result.replace(pos, 2, oss.str());
            return format_fallback(result, std::forward<Args>(args)...);
        }
        return result;
    }
};

// Convenient macros for the current logger instance
#define LOG_TRACE(...) if(auto log = Logger::instance_) log->trace(__VA_ARGS__)
#define LOG_DEBUG(...) if(auto log = Logger::instance_) log->debug(__VA_ARGS__)
#define LOG_INFO(...) if(auto log = Logger::instance_) log->info(__VA_ARGS__)
#define LOG_WARN(...) if(auto log = Logger::instance_) log->warn(__VA_ARGS__)
#define LOG_ERROR(...) if(auto log = Logger::instance_) log->error(__VA_ARGS__)
#define LOG_CRITICAL(...) if(auto log = Logger::instance_) log->critical(__VA_ARGS__)
