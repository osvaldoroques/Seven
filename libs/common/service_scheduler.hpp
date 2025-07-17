#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <vector>
#include <queue>
#include <string>
#include <condition_variable>

// Forward declarations
class ThreadPool;
class Logger;

/**
 * @brief Lightweight, in-process scheduler for ServiceHost
 * 
 * Features:
 * - Cron-like scheduling with human-readable expressions
 * - Interval-based scheduling (every N seconds/minutes)
 * - One-time delayed execution
 * - Thread-safe task management
 * - Automatic cleanup of completed tasks
 * - Integration with existing ThreadPool
 * - No blocking of NATS threads
 * 
 * Use cases:
 * - Metrics flush (batch and push metrics)
 * - Cache cleanup (purge expired entries)
 * - Health-check heartbeats
 * - Back-pressure monitoring
 * - Periodic state cleanup
 */
class ServiceScheduler {
public:
    using TaskId = size_t;
    using TaskFunction = std::function<void()>;
    
    // Task execution modes
    enum class ExecutionMode {
        RECURRING,   // Repeat based on schedule
        ONE_TIME,    // Execute once and remove
        CONDITIONAL  // Execute if condition is met
    };
    
    // Task configuration
    struct TaskConfig {
        std::string name;
        ExecutionMode mode = ExecutionMode::RECURRING;
        bool enabled = true;
        std::chrono::milliseconds timeout = std::chrono::milliseconds(5000);
        int max_retries = 3;
        std::function<bool()> condition = nullptr; // For conditional tasks
        
        // Static method to create default config
        static TaskConfig create_default() {
            TaskConfig config;
            config.mode = ExecutionMode::RECURRING;
            config.enabled = true;
            config.timeout = std::chrono::milliseconds(5000);
            config.max_retries = 3;
            config.condition = nullptr;
            return config;
        }
    };
    
    // Task statistics
    struct TaskStats {
        std::string name;
        size_t executions = 0;
        size_t failures = 0;
        std::chrono::milliseconds avg_duration{0};
        std::chrono::steady_clock::time_point last_execution;
        std::chrono::steady_clock::time_point next_execution;
        bool enabled = true;
    };

private:
    // Internal task representation
    struct ScheduledTask {
        TaskId id;
        TaskConfig config;
        TaskFunction function;
        std::chrono::steady_clock::time_point next_run;
        std::chrono::milliseconds interval;
        TaskStats stats;
        std::atomic<bool> running{false};
        
        ScheduledTask(TaskId task_id, const TaskConfig& cfg, TaskFunction func, 
                     std::chrono::milliseconds inter)
            : id(task_id), config(cfg), function(std::move(func)), interval(inter) {
            stats.name = config.name;
            stats.enabled = config.enabled;
            calculate_next_run();
        }
        
        void calculate_next_run() {
            next_run = std::chrono::steady_clock::now() + interval;
            stats.next_execution = next_run;
        }
        
        bool is_ready() const {
            return config.enabled && 
                   std::chrono::steady_clock::now() >= next_run &&
                   !running.load();
        }
        
        bool should_execute() const {
            if (!is_ready()) return false;
            if (config.condition) {
                return config.condition();
            }
            return true;
        }
    };
    
    ThreadPool* thread_pool_;
    std::shared_ptr<Logger> logger_;
    std::vector<std::unique_ptr<ScheduledTask>> tasks_;
    mutable std::mutex tasks_mutex_;
    std::atomic<bool> running_{false};
    std::atomic<TaskId> next_task_id_{1};
    std::thread scheduler_thread_;
    std::condition_variable cv_;
    std::mutex cv_mutex_;
    
    // Performance monitoring
    std::atomic<size_t> total_executions_{0};
    std::atomic<size_t> total_failures_{0};
    std::chrono::steady_clock::time_point start_time_;
    
public:
    explicit ServiceScheduler(ThreadPool* pool, std::shared_ptr<Logger> logger);
    ~ServiceScheduler();
    
    // Disable copy/move
    ServiceScheduler(const ServiceScheduler&) = delete;
    ServiceScheduler& operator=(const ServiceScheduler&) = delete;
    
    // Start the scheduler
    void start();
    
    // Stop the scheduler
    void stop();
    
    // Schedule a task to run every N seconds
    // Schedule a task to run at specified intervals
    TaskId schedule_interval(const std::string& name, 
                           std::chrono::milliseconds interval,
                           TaskFunction task);
    TaskId schedule_interval(const std::string& name, 
                           std::chrono::milliseconds interval,
                           TaskFunction task,
                           const TaskConfig& config);
    
    // Schedule a task to run every N minutes
    TaskId schedule_every_minutes(const std::string& name,
                                int minutes,
                                TaskFunction task);
    TaskId schedule_every_minutes(const std::string& name,
                                int minutes,
                                TaskFunction task,
                                const TaskConfig& config);
    
    // Schedule a task to run every N hours
    TaskId schedule_every_hours(const std::string& name,
                              int hours,
                              TaskFunction task);
    TaskId schedule_every_hours(const std::string& name,
                              int hours,
                              TaskFunction task,
                              const TaskConfig& config);
    
    // Schedule a one-time task to run after delay
    TaskId schedule_once(const std::string& name,
                        std::chrono::milliseconds delay,
                        TaskFunction task);
    TaskId schedule_once(const std::string& name,
                        std::chrono::milliseconds delay,
                        TaskFunction task,
                        const TaskConfig& config);
    
    // Schedule a conditional task (runs when condition is true)
    TaskId schedule_conditional(const std::string& name,
                              std::chrono::milliseconds check_interval,
                              std::function<bool()> condition,
                              TaskFunction task);
    TaskId schedule_conditional(const std::string& name,
                              std::chrono::milliseconds check_interval,
                              std::function<bool()> condition,
                              TaskFunction task,
                              const TaskConfig& config);
    
    // Task management
    bool cancel_task(TaskId id);
    bool enable_task(TaskId id);
    bool disable_task(TaskId id);
    bool is_task_running(TaskId id) const;
    
    // Get task statistics
    std::vector<TaskStats> get_task_stats() const;
    TaskStats get_task_stats(TaskId id) const;
    
    // Scheduler statistics
    struct SchedulerStats {
        size_t active_tasks = 0;
        size_t total_executions = 0;
        size_t total_failures = 0;
        double failure_rate = 0.0;
        std::chrono::milliseconds uptime{0};
    };
    
    SchedulerStats get_scheduler_stats() const;
    
    // Convenience methods for common patterns
    
    // Schedule metrics flush every 30 seconds
    TaskId schedule_metrics_flush(TaskFunction flush_func) {
        return schedule_interval("metrics_flush", std::chrono::seconds(30), 
                               std::move(flush_func));
    }
    
    // Schedule cache cleanup every 5 minutes
    TaskId schedule_cache_cleanup(TaskFunction cleanup_func) {
        return schedule_every_minutes("cache_cleanup", 5, std::move(cleanup_func));
    }
    
    // Schedule health check heartbeat every 10 seconds
    TaskId schedule_health_heartbeat(TaskFunction heartbeat_func) {
        return schedule_interval("health_heartbeat", std::chrono::seconds(10),
                               std::move(heartbeat_func));
    }
    
    // Schedule back-pressure monitoring every 1 second
    TaskId schedule_backpressure_monitor(std::function<size_t()> queue_size_func,
                                        size_t threshold,
                                        TaskFunction alert_func) {
        TaskConfig config;
        config.name = "backpressure_monitor";
        config.condition = [queue_size_func, threshold]() {
            return queue_size_func() > threshold;
        };
        
        return schedule_conditional("backpressure_monitor", std::chrono::seconds(1),
                                  config.condition, std::move(alert_func), config);
    }
    
private:
    void scheduler_loop();
    void execute_task(std::unique_ptr<ScheduledTask>& task);
    void cleanup_completed_tasks();
    std::chrono::milliseconds get_next_wake_time() const;
};
