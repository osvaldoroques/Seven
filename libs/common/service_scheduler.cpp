#include "service_scheduler.hpp"
#include "thread_pool.hpp"
#include "logger.hpp"
#include <algorithm>
#include <sstream>

ServiceScheduler::ServiceScheduler(ThreadPool* pool, std::shared_ptr<Logger> logger)
    : thread_pool_(pool), logger_(logger), start_time_(std::chrono::steady_clock::now()) {
    logger_->debug("ServiceScheduler initialized");
}

ServiceScheduler::~ServiceScheduler() {
    stop();
}

void ServiceScheduler::start() {
    if (running_.exchange(true)) {
        logger_->warn("ServiceScheduler already running");
        return;
    }
    
    logger_->info("Starting ServiceScheduler");
    scheduler_thread_ = std::thread(&ServiceScheduler::scheduler_loop, this);
}

void ServiceScheduler::stop() {
    if (!running_.exchange(false)) {
        return; // Already stopped
    }
    
    logger_->info("Stopping ServiceScheduler");
    
    // Wake up scheduler thread
    cv_.notify_all();
    
    // Wait for scheduler thread to finish
    if (scheduler_thread_.joinable()) {
        scheduler_thread_.join();
    }
    
    // Cancel all running tasks
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    for (auto& task : tasks_) {
        task->config.enabled = false;
    }
    
    logger_->info("ServiceScheduler stopped");
}

ServiceScheduler::TaskId ServiceScheduler::schedule_interval(
    const std::string& name,
    std::chrono::milliseconds interval,
    TaskFunction task) {
    
    return schedule_interval(name, interval, std::move(task), TaskConfig::create_default());
}

ServiceScheduler::TaskId ServiceScheduler::schedule_interval(
    const std::string& name,
    std::chrono::milliseconds interval,
    TaskFunction task,
    const TaskConfig& config) {
    
    TaskId id = next_task_id_.fetch_add(1);
    
    TaskConfig final_config = config;
    final_config.name = name;
    
    auto scheduled_task = std::make_unique<ScheduledTask>(id, final_config, 
                                                         std::move(task), interval);
    
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    tasks_.push_back(std::move(scheduled_task));
    
    logger_->debug("Scheduled interval task: {} ({}ms interval)", name, interval.count());
    cv_.notify_one();
    
    return id;
}

ServiceScheduler::TaskId ServiceScheduler::schedule_every_minutes(
    const std::string& name,
    int minutes,
    TaskFunction task) {
    
    return schedule_every_minutes(name, minutes, std::move(task), TaskConfig::create_default());
}

ServiceScheduler::TaskId ServiceScheduler::schedule_every_minutes(
    const std::string& name,
    int minutes,
    TaskFunction task,
    const TaskConfig& config) {
    
    return schedule_interval(name, std::chrono::minutes(minutes), std::move(task), config);
}

ServiceScheduler::TaskId ServiceScheduler::schedule_every_hours(
    const std::string& name,
    int hours,
    TaskFunction task) {
    
    return schedule_every_hours(name, hours, std::move(task), TaskConfig::create_default());
}

ServiceScheduler::TaskId ServiceScheduler::schedule_every_hours(
    const std::string& name,
    int hours,
    TaskFunction task,
    const TaskConfig& config) {
    
    return schedule_interval(name, std::chrono::hours(hours), std::move(task), config);
}

ServiceScheduler::TaskId ServiceScheduler::schedule_once(
    const std::string& name,
    std::chrono::milliseconds delay,
    TaskFunction task) {
    
    return schedule_once(name, delay, std::move(task), TaskConfig::create_default());
}

ServiceScheduler::TaskId ServiceScheduler::schedule_once(
    const std::string& name,
    std::chrono::milliseconds delay,
    TaskFunction task,
    const TaskConfig& config) {
    
    TaskConfig final_config = config;
    final_config.name = name;
    final_config.mode = ExecutionMode::ONE_TIME;
    
    return schedule_interval(name, delay, std::move(task), final_config);
}

ServiceScheduler::TaskId ServiceScheduler::schedule_conditional(
    const std::string& name,
    std::chrono::milliseconds check_interval,
    std::function<bool()> condition,
    TaskFunction task) {
    
    return schedule_conditional(name, check_interval, std::move(condition), std::move(task), TaskConfig::create_default());
}

ServiceScheduler::TaskId ServiceScheduler::schedule_conditional(
    const std::string& name,
    std::chrono::milliseconds check_interval,
    std::function<bool()> condition,
    TaskFunction task,
    const TaskConfig& config) {
    
    TaskConfig final_config = config;
    final_config.name = name;
    final_config.mode = ExecutionMode::CONDITIONAL;
    final_config.condition = std::move(condition);
    
    return schedule_interval(name, check_interval, std::move(task), final_config);
}

bool ServiceScheduler::cancel_task(TaskId id) {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    
    auto it = std::find_if(tasks_.begin(), tasks_.end(),
                          [id](const std::unique_ptr<ScheduledTask>& task) {
                              return task->id == id;
                          });
    
    if (it != tasks_.end()) {
        logger_->debug("Cancelling task: {}", (*it)->config.name);
        tasks_.erase(it);
        return true;
    }
    
    return false;
}

bool ServiceScheduler::enable_task(TaskId id) {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    
    auto it = std::find_if(tasks_.begin(), tasks_.end(),
                          [id](const std::unique_ptr<ScheduledTask>& task) {
                              return task->id == id;
                          });
    
    if (it != tasks_.end()) {
        (*it)->config.enabled = true;
        (*it)->stats.enabled = true;
        logger_->debug("Enabled task: {}", (*it)->config.name);
        cv_.notify_one();
        return true;
    }
    
    return false;
}

bool ServiceScheduler::disable_task(TaskId id) {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    
    auto it = std::find_if(tasks_.begin(), tasks_.end(),
                          [id](const std::unique_ptr<ScheduledTask>& task) {
                              return task->id == id;
                          });
    
    if (it != tasks_.end()) {
        (*it)->config.enabled = false;
        (*it)->stats.enabled = false;
        logger_->debug("Disabled task: {}", (*it)->config.name);
        return true;
    }
    
    return false;
}

bool ServiceScheduler::is_task_running(TaskId id) const {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    
    auto it = std::find_if(tasks_.begin(), tasks_.end(),
                          [id](const std::unique_ptr<ScheduledTask>& task) {
                              return task->id == id;
                          });
    
    return it != tasks_.end() && (*it)->running.load();
}

std::vector<ServiceScheduler::TaskStats> ServiceScheduler::get_task_stats() const {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    
    std::vector<TaskStats> stats;
    stats.reserve(tasks_.size());
    
    for (const auto& task : tasks_) {
        stats.push_back(task->stats);
    }
    
    return stats;
}

ServiceScheduler::TaskStats ServiceScheduler::get_task_stats(TaskId id) const {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    
    auto it = std::find_if(tasks_.begin(), tasks_.end(),
                          [id](const std::unique_ptr<ScheduledTask>& task) {
                              return task->id == id;
                          });
    
    if (it != tasks_.end()) {
        return (*it)->stats;
    }
    
    return {}; // Return empty stats if task not found
}

ServiceScheduler::SchedulerStats ServiceScheduler::get_scheduler_stats() const {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_);
    
    size_t active_tasks = 0;
    for (const auto& task : tasks_) {
        if (task->config.enabled) {
            active_tasks++;
        }
    }
    
    size_t total_exec = total_executions_.load();
    size_t total_fail = total_failures_.load();
    double failure_rate = total_exec > 0 ? static_cast<double>(total_fail) / total_exec : 0.0;
    
    return SchedulerStats{
        .active_tasks = active_tasks,
        .total_executions = total_exec,
        .total_failures = total_fail,
        .failure_rate = failure_rate,
        .uptime = uptime
    };
}

void ServiceScheduler::scheduler_loop() {
    logger_->debug("Scheduler loop started");
    
    while (running_.load()) {
        try {
            // Check for ready tasks
            std::vector<std::unique_ptr<ScheduledTask>*> ready_tasks;
            
            {
                std::lock_guard<std::mutex> lock(tasks_mutex_);
                for (auto& task : tasks_) {
                    if (task->should_execute()) {
                        ready_tasks.push_back(&task);
                    }
                }
            }
            
            // Execute ready tasks
            for (auto* task_ptr : ready_tasks) {
                execute_task(*task_ptr);
            }
            
            // Cleanup completed one-time tasks
            cleanup_completed_tasks();
            
            // Sleep until next task is ready
            auto next_wake = get_next_wake_time();
            
            std::unique_lock<std::mutex> cv_lock(cv_mutex_);
            cv_.wait_for(cv_lock, next_wake, [this] { return !running_.load(); });
            
        } catch (const std::exception& e) {
            logger_->error("Scheduler loop error: {}", e.what());
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    logger_->debug("Scheduler loop stopped");
}

void ServiceScheduler::execute_task(std::unique_ptr<ScheduledTask>& task) {
    if (!task->config.enabled || task->running.load()) {
        return;
    }
    
    task->running.store(true);
    auto start_time = std::chrono::steady_clock::now();
    
    // Submit to thread pool
    thread_pool_->submit([this, &task, start_time]() {
        try {
            logger_->trace("Executing scheduled task: {}", task->config.name);
            
            // Execute the task
            task->function();
            
            // Update statistics
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            task->stats.executions++;
            task->stats.last_execution = start_time;
            
            // Update average duration
            if (task->stats.executions == 1) {
                task->stats.avg_duration = duration;
            } else {
                auto total_duration = task->stats.avg_duration * (task->stats.executions - 1) + duration;
                task->stats.avg_duration = total_duration / task->stats.executions;
            }
            
            total_executions_.fetch_add(1);
            
            logger_->trace("Task completed: {} ({}ms)", task->config.name, duration.count());
            
        } catch (const std::exception& e) {
            task->stats.failures++;
            total_failures_.fetch_add(1);
            logger_->error("Task failed: {} - {}", task->config.name, e.what());
        } catch (...) {
            task->stats.failures++;
            total_failures_.fetch_add(1);
            logger_->error("Task failed with unknown exception: {}", task->config.name);
        }
        
        // Schedule next execution for recurring tasks
        if (task->config.mode == ExecutionMode::RECURRING) {
            task->calculate_next_run();
        }
        
        task->running.store(false);
    });
}

void ServiceScheduler::cleanup_completed_tasks() {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    
    tasks_.erase(
        std::remove_if(tasks_.begin(), tasks_.end(),
                      [](const std::unique_ptr<ScheduledTask>& task) {
                          return task->config.mode == ExecutionMode::ONE_TIME &&
                                 task->stats.executions > 0 &&
                                 !task->running.load();
                      }),
        tasks_.end()
    );
}

std::chrono::milliseconds ServiceScheduler::get_next_wake_time() const {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto next_time = now + std::chrono::minutes(1); // Default to 1 minute
    
    for (const auto& task : tasks_) {
        if (task->config.enabled && task->next_run < next_time) {
            next_time = task->next_run;
        }
    }
    
    auto sleep_duration = next_time - now;
    return std::chrono::duration_cast<std::chrono::milliseconds>(sleep_duration);
}
