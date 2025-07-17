# ServiceScheduler - Lightweight Task Scheduler for ServiceHost

## Overview

ServiceScheduler is a lightweight, in-process task scheduler integrated into the ServiceHost framework. It provides cron-like scheduling capabilities without blocking NATS message processing threads.

## Features

- **🔄 Recurring Tasks**: Schedule tasks to run at regular intervals
- **⏰ One-time Tasks**: Execute tasks once after a delay
- **🎯 Conditional Tasks**: Execute tasks only when conditions are met
- **📊 Statistics**: Track task execution, failures, and performance
- **🔧 Thread-Safe**: Safe for concurrent access across multiple threads
- **⚡ Non-blocking**: Uses existing ThreadPool, doesn't block NATS threads
- **🧹 Auto-cleanup**: Automatically removes completed one-time tasks

## Common Use Cases

### 1. Metrics Flush
```cpp
// Schedule metrics flush every 30 seconds
host.schedule_metrics_flush([this]() {
    // Collect and send metrics to monitoring system
    metrics_collector.flush_to_prometheus();
});
```

### 2. Cache Cleanup
```cpp
// Schedule cache cleanup every 5 minutes
host.schedule_cache_cleanup([this]() {
    host.get_cache().cleanup_expired();
    logger_->debug("Cache cleanup completed");
});
```

### 3. Health Check Heartbeat
```cpp
// Send health status every 10 seconds
host.schedule_health_heartbeat([this]() {
    HealthStatus status = collect_health_status();
    publish_health_status(status);
});
```

### 4. Back-pressure Monitoring
```cpp
// Monitor queue size and react to high load
host.schedule_backpressure_monitor(
    [this]() -> size_t { 
        return thread_pool.pending_tasks(); 
    },
    100,  // Threshold
    [this]() { 
        logger_->warn("High queue size detected!");
        // Implement backpressure handling
    }
);
```

## Advanced Usage

### Custom Scheduling
```cpp
// Schedule a custom task every 2 hours
TaskId task_id = host.schedule_interval(
    "cleanup_old_logs", 
    std::chrono::hours(2),
    [this]() {
        cleanup_old_log_files();
    }
);

// Schedule a one-time startup task
host.schedule_once(
    "send_startup_notification",
    std::chrono::seconds(30),
    [this]() {
        send_startup_notification();
    }
);
```

### Task Management
```cpp
// Get scheduler reference
auto& scheduler = host.get_scheduler();

// Disable a task temporarily
scheduler.disable_task(task_id);

// Re-enable the task
scheduler.enable_task(task_id);

// Cancel a task completely
scheduler.cancel_task(task_id);

// Check if task is running
bool running = scheduler.is_task_running(task_id);
```

### Statistics and Monitoring
```cpp
// Get overall scheduler statistics
auto stats = scheduler.get_scheduler_stats();
logger_->info("Scheduler: {} active tasks, {:.2f}% failure rate", 
              stats.active_tasks, stats.failure_rate * 100);

// Get statistics for all tasks
auto task_stats = scheduler.get_task_stats();
for (const auto& stat : task_stats) {
    logger_->info("Task {}: {} executions, {} failures", 
                  stat.name, stat.executions, stat.failures);
}
```

## Configuration

Tasks can be configured with various options:

```cpp
ServiceScheduler::TaskConfig config;
config.enabled = true;                              // Enable/disable task
config.timeout = std::chrono::seconds(30);          // Task timeout
config.max_retries = 3;                             // Retry on failure
config.mode = ServiceScheduler::ExecutionMode::RECURRING;  // Execution mode

TaskId id = scheduler.schedule_interval("my_task", 
                                       std::chrono::minutes(5), 
                                       my_function, 
                                       config);
```

## Best Practices

### 1. Keep Tasks Short
```cpp
// ✅ Good: Quick operations
host.schedule_interval("metrics_flush", std::chrono::seconds(30), [this]() {
    metrics.flush();  // Fast operation
});

// ❌ Bad: Long-running operations
host.schedule_interval("heavy_computation", std::chrono::seconds(10), [this]() {
    process_large_dataset();  // This blocks the thread pool
});
```

### 2. Use Appropriate Intervals
```cpp
// ✅ Good: Reasonable intervals
host.schedule_cache_cleanup([this]() { cache.cleanup(); });  // 5 minutes default

// ❌ Bad: Too frequent
host.schedule_interval("frequent_task", std::chrono::milliseconds(100), []() {
    // This runs 10 times per second - too frequent!
});
```

### 3. Handle Failures Gracefully
```cpp
host.schedule_interval("network_task", std::chrono::minutes(1), [this]() {
    try {
        make_network_call();
    } catch (const std::exception& e) {
        logger_->error("Network task failed: {}", e.what());
        // Don't let exceptions kill the scheduler
    }
});
```

### 4. Monitor Task Performance
```cpp
// Check task statistics periodically
host.schedule_interval("monitor_tasks", std::chrono::minutes(10), [this]() {
    auto stats = host.get_scheduler().get_task_stats();
    for (const auto& stat : stats) {
        if (stat.failures > 0) {
            logger_->warn("Task {} has {} failures", stat.name, stat.failures);
        }
    }
});
```

## Integration with ServiceHost

The scheduler is automatically initialized when ServiceHost starts:

```cpp
ServiceHost host("my-service", "portfolio-manager");
// Scheduler is automatically started and ready to use

// Schedule tasks
host.schedule_metrics_flush([this]() { flush_metrics(); });
host.schedule_cache_cleanup([this]() { cleanup_cache(); });

// ServiceHost automatically stops scheduler on destruction
```

## Thread Safety

ServiceScheduler is fully thread-safe:
- Multiple threads can schedule tasks concurrently
- Task execution is handled by the existing ThreadPool
- Statistics are updated atomically
- No locks held during task execution

## Performance Characteristics

- **Scheduling Overhead**: Minimal - tasks are stored in memory
- **Execution Overhead**: Uses existing ThreadPool, no additional threads
- **Memory Usage**: ~100 bytes per scheduled task
- **CPU Usage**: Scheduler thread wakes up only when tasks are ready

## Comparison with Alternatives

| Feature | ServiceScheduler | Cron | Timer Threads |
|---------|------------------|------|---------------|
| In-process | ✅ | ❌ | ✅ |
| Thread-safe | ✅ | ❌ | ⚠️ |
| Integration | ✅ | ❌ | ❌ |
| Statistics | ✅ | ❌ | ❌ |
| Conditional | ✅ | ❌ | ❌ |
| Lightweight | ✅ | ❌ | ⚠️ |

ServiceScheduler is specifically designed for microservices that need lightweight, integrated task scheduling without external dependencies or additional processes.
