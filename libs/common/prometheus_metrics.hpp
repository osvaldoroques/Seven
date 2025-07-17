#pragma once

#include <string>
#include <unordered_map>
#include <atomic>
#include <chrono>
#include <mutex>
#include <vector>
#include <sstream>
#include <memory>

namespace PrometheusMetrics {

// Basic metric types
enum class MetricType {
    COUNTER,
    GAUGE,
    HISTOGRAM,
    SUMMARY
};

// Base metric class
class Metric {
public:
    Metric(const std::string& name, const std::string& help, MetricType type)
        : name_(name), help_(help), type_(type) {}
    
    virtual ~Metric() = default;
    virtual std::string serialize() const = 0;
    
    const std::string& name() const { return name_; }
    const std::string& help() const { return help_; }
    MetricType type() const { return type_; }

protected:
    std::string name_;
    std::string help_;
    MetricType type_;
};

// Counter metric (always increases)
class Counter : public Metric {
public:
    Counter(const std::string& name, const std::string& help, 
            const std::unordered_map<std::string, std::string>& labels = {})
        : Metric(name, help, MetricType::COUNTER), labels_(labels), value_(0) {}
    
    void inc(double amount = 1.0) {
        double old_value = value_.load();
        while (!value_.compare_exchange_weak(old_value, old_value + amount)) {
            // Retry until successful
        }
    }
    
    double value() const {
        return value_.load();
    }
    
    std::string serialize() const override {
        std::stringstream ss;
        ss << "# HELP " << name_ << " " << help_ << "\n";
        ss << "# TYPE " << name_ << " counter\n";
        ss << name_;
        
        if (!labels_.empty()) {
            ss << "{";
            bool first = true;
            for (const auto& [key, val] : labels_) {
                if (!first) ss << ",";
                ss << key << "=\"" << val << "\"";
                first = false;
            }
            ss << "}";
        }
        
        ss << " " << value() << "\n";
        return ss.str();
    }

private:
    std::unordered_map<std::string, std::string> labels_;
    std::atomic<double> value_;
};

// Gauge metric (can increase or decrease)
class Gauge : public Metric {
public:
    Gauge(const std::string& name, const std::string& help,
          const std::unordered_map<std::string, std::string>& labels = {})
        : Metric(name, help, MetricType::GAUGE), labels_(labels), value_(0) {}
    
    void set(double value) {
        value_.store(value);
    }
    
    void inc(double amount = 1.0) {
        double old_value = value_.load();
        while (!value_.compare_exchange_weak(old_value, old_value + amount)) {
            // Retry until successful
        }
    }
    
    void dec(double amount = 1.0) {
        double old_value = value_.load();
        while (!value_.compare_exchange_weak(old_value, old_value - amount)) {
            // Retry until successful
        }
    }
    
    double value() const {
        return value_.load();
    }
    
    std::string serialize() const override {
        std::stringstream ss;
        ss << "# HELP " << name_ << " " << help_ << "\n";
        ss << "# TYPE " << name_ << " gauge\n";
        ss << name_;
        
        if (!labels_.empty()) {
            ss << "{";
            bool first = true;
            for (const auto& [key, val] : labels_) {
                if (!first) ss << ",";
                ss << key << "=\"" << val << "\"";
                first = false;
            }
            ss << "}";
        }
        
        ss << " " << value() << "\n";
        return ss.str();
    }

private:
    std::unordered_map<std::string, std::string> labels_;
    std::atomic<double> value_;
};

// Simple histogram implementation
class Histogram : public Metric {
public:
    Histogram(const std::string& name, const std::string& help,
              const std::vector<double>& buckets = {0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0},
              const std::unordered_map<std::string, std::string>& labels = {})
        : Metric(name, help, MetricType::HISTOGRAM), labels_(labels), buckets_(buckets), count_(0), sum_(0) {
        
        for (double bucket : buckets_) {
            bucket_counts_[bucket] = 0;
        }
    }
    
    void observe(double value) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Increment count
        uint64_t old_count = count_.load();
        while (!count_.compare_exchange_weak(old_count, old_count + 1)) {
            // Retry until successful
        }
        
        // Add to sum
        double old_sum = sum_.load();
        while (!sum_.compare_exchange_weak(old_sum, old_sum + value)) {
            // Retry until successful
        }
        
        for (auto& [bucket, count] : bucket_counts_) {
            if (value <= bucket) {
                count.fetch_add(1);
            }
        }
    }
    
    std::string serialize() const override {
        std::stringstream ss;
        ss << "# HELP " << name_ << " " << help_ << "\n";
        ss << "# TYPE " << name_ << " histogram\n";
        
        // Bucket counts
        for (const auto& [bucket, count] : bucket_counts_) {
            ss << name_ << "_bucket";
            if (!labels_.empty()) {
                ss << "{";
                bool first = true;
                for (const auto& [key, val] : labels_) {
                    if (!first) ss << ",";
                    ss << key << "=\"" << val << "\"";
                    first = false;
                }
                if (!first) ss << ",";
                ss << "le=\"" << bucket << "\"";
                ss << "}";
            } else {
                ss << "{le=\"" << bucket << "\"}";
            }
            ss << " " << count.load() << "\n";
        }
        
        // +Inf bucket
        ss << name_ << "_bucket";
        if (!labels_.empty()) {
            ss << "{";
            bool first = true;
            for (const auto& [key, val] : labels_) {
                if (!first) ss << ",";
                ss << key << "=\"" << val << "\"";
                first = false;
            }
            if (!first) ss << ",";
            ss << "le=\"+Inf\"";
            ss << "}";
        } else {
            ss << "{le=\"+Inf\"}";
        }
        ss << " " << count_.load() << "\n";
        
        // Count and sum
        ss << name_ << "_count " << count_.load() << "\n";
        ss << name_ << "_sum " << sum_.load() << "\n";
        
        return ss.str();
    }

private:
    std::unordered_map<std::string, std::string> labels_;
    std::vector<double> buckets_;
    std::unordered_map<double, std::atomic<uint64_t>> bucket_counts_;
    std::atomic<uint64_t> count_;
    std::atomic<double> sum_;
    mutable std::mutex mutex_;
};

// Metrics registry
class MetricsRegistry {
public:
    static MetricsRegistry& instance() {
        static MetricsRegistry registry;
        return registry;
    }
    
    void register_metric(std::shared_ptr<Metric> metric) {
        std::lock_guard<std::mutex> lock(mutex_);
        metrics_[metric->name()] = metric;
    }
    
    std::shared_ptr<Counter> create_counter(const std::string& name, const std::string& help,
                                           const std::unordered_map<std::string, std::string>& labels = {}) {
        auto counter = std::make_shared<Counter>(name, help, labels);
        register_metric(counter);
        return counter;
    }
    
    std::shared_ptr<Gauge> create_gauge(const std::string& name, const std::string& help,
                                       const std::unordered_map<std::string, std::string>& labels = {}) {
        auto gauge = std::make_shared<Gauge>(name, help, labels);
        register_metric(gauge);
        return gauge;
    }
    
    std::shared_ptr<Histogram> create_histogram(const std::string& name, const std::string& help,
                                               const std::vector<double>& buckets = {0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0},
                                               const std::unordered_map<std::string, std::string>& labels = {}) {
        auto histogram = std::make_shared<Histogram>(name, help, buckets, labels);
        register_metric(histogram);
        return histogram;
    }
    
    std::string serialize_all() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::stringstream ss;
        
        for (const auto& [name, metric] : metrics_) {
            ss << metric->serialize() << "\n";
        }
        
        return ss.str();
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        metrics_.clear();
    }

private:
    MetricsRegistry() = default;
    std::unordered_map<std::string, std::shared_ptr<Metric>> metrics_;
    mutable std::mutex mutex_;
};

// RAII timer for measuring duration
class Timer {
public:
    Timer(std::shared_ptr<Histogram> histogram) 
        : histogram_(histogram), start_time_(std::chrono::high_resolution_clock::now()) {}
    
    ~Timer() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time_);
        histogram_->observe(duration.count() / 1000000.0); // Convert to seconds
    }

private:
    std::shared_ptr<Histogram> histogram_;
    std::chrono::high_resolution_clock::time_point start_time_;
};

// Macro for easy timing
#define PROMETHEUS_TIMER(histogram) PrometheusMetrics::Timer _timer(histogram)

} // namespace PrometheusMetrics
