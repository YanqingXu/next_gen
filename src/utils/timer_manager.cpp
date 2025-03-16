#include "../../include/utils/timer.h"

namespace next_gen {

TimerManager& TimerManager::instance() {
    static TimerManager instance;
    return instance;
}

void TimerManager::start() {
    if (running_.exchange(true)) {
        return; // Already running
    }
    
    worker_thread_ = std::thread(&TimerManager::run, this);
    Logger::info("Timer manager started");
}

void TimerManager::stop() {
    if (!running_.exchange(false)) {
        return; // Already stopped
    }
    
    cv_.notify_all();
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    tasks_.clear();
    while (!queue_.empty()) {
        queue_.pop();
    }
    groups_.clear();
    timer_to_group_.clear();
    
    Logger::info("Timer manager stopped");
}

TimerId TimerManager::createOnce(u64 delay_ms, std::function<void()> callback) {
    return createTimer(delay_ms, 0, false, std::move(callback));
}

TimerId TimerManager::createRepeat(u64 delay_ms, u64 interval_ms, std::function<void()> callback) {
    return createTimer(delay_ms, interval_ms, true, std::move(callback));
}

TimerId TimerManager::createTimer(u64 delay_ms, u64 interval_ms, bool repeat, std::function<void()> callback) {
    if (!callback) {
        Logger::warning("Timer created with null callback");
        return 0;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    TimerId id = next_id_++;
    if (id == 0) id = next_id_++; // Skip ID 0, it's reserved for invalid timer ID
    
    u64 now = getCurrentTimeMillis();
    TimerTask task = {
        id,
        now + delay_ms,
        interval_ms,
        repeat,
        std::move(callback)
    };
    
    tasks_[id] = task;
    queue_.push(task);
    
    cv_.notify_one();
    return id;
}

bool TimerManager::cancel(TimerId id) {
    if (id == 0) return false;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = tasks_.find(id);
    if (it == tasks_.end()) {
        return false;
    }
    
    tasks_.erase(it);
    
    // Remove from group if it belongs to one
    auto group_it = timer_to_group_.find(id);
    if (group_it != timer_to_group_.end()) {
        TimerGroupId group_id = group_it->second;
        timer_to_group_.erase(group_it);
        
        auto& group_timers = groups_[group_id];
        group_timers.erase(
            std::remove(group_timers.begin(), group_timers.end(), id),
            group_timers.end()
        );
        
        if (group_timers.empty()) {
            groups_.erase(group_id);
        }
    }
    
    // We don't remove it from the priority queue immediately for efficiency,
    // it will be skipped when it's processed
    return true;
}

bool TimerManager::modify(TimerId id, u64 delay_ms, u64 interval_ms, bool repeat) {
    if (id == 0) return false;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = tasks_.find(id);
    if (it == tasks_.end()) {
        return false;
    }
    
    u64 now = getCurrentTimeMillis();
    it->second.next_run = now + delay_ms;
    it->second.interval = interval_ms;
    it->second.repeat = repeat;
    
    // We need to rebuild the priority queue to reflect these changes
    rebuildQueue();
    cv_.notify_one();
    
    return true;
}

bool TimerManager::exists(TimerId id) const {
    if (id == 0) return false;
    
    std::lock_guard<std::mutex> lock(mutex_);
    return tasks_.find(id) != tasks_.end();
}

size_t TimerManager::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tasks_.size();
}

void TimerManager::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    tasks_.clear();
    while (!queue_.empty()) {
        queue_.pop();
    }
    groups_.clear();
    timer_to_group_.clear();
}

void TimerManager::rebuildQueue() {
    while (!queue_.empty()) {
        queue_.pop();
    }
    
    for (const auto& pair : tasks_) {
        queue_.push(pair.second);
    }
}

u64 TimerManager::getCurrentTimeMillis() const {
    using namespace std::chrono;
    return duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()
    ).count();
}

void TimerManager::run() {
    while (running_) {
        std::vector<TimerTask> expired_tasks;
        
        {
            std::unique_lock<std::mutex> lock(mutex_);
            
            if (tasks_.empty()) {
                // Wait for a new timer or stop signal
                cv_.wait(lock, [this] { return !running_ || !tasks_.empty(); });
                if (!running_) break;
                continue;
            }
            
            u64 now = getCurrentTimeMillis();
            
            // Calculate wait time until next task
            u64 wait_time = 0;
            if (!queue_.empty() && queue_.top().next_run > now) {
                wait_time = queue_.top().next_run - now;
            }
            
            if (wait_time > 0) {
                // Wait until next task is due or a new timer is added
                auto status = cv_.wait_for(lock, std::chrono::milliseconds(wait_time), 
                    [this, &now, &wait_time] {
                        if (!running_) return true;
                        if (queue_.empty()) return false;
                        
                        // Check if a new timer was added that should run sooner
                        u64 current_now = getCurrentTimeMillis();
                        u64 next_run = queue_.top().next_run;
                        return next_run <= current_now;
                    });
                
                if (!running_) break;
                if (!status) continue; // Spurious wakeup, continue waiting
                
                now = getCurrentTimeMillis();
            }
            
            // Get all expired tasks
            while (!queue_.empty() && queue_.top().next_run <= now) {
                TimerTask task = queue_.top();
                queue_.pop();
                
                // Check if the task still exists (it might have been cancelled)
                auto it = tasks_.find(task.id);
                if (it != tasks_.end()) {
                    // Add to expired tasks list
                    expired_tasks.push_back(task);
                    
                    // If repeating, schedule next run
                    if (task.repeat) {
                        task.next_run = now + task.interval;
                        it->second.next_run = task.next_run;
                        queue_.push(task);
                    } else {
                        // Remove one-time task
                        tasks_.erase(it);
                        
                        // Remove from group if it belongs to one
                        auto group_it = timer_to_group_.find(task.id);
                        if (group_it != timer_to_group_.end()) {
                            TimerGroupId group_id = group_it->second;
                            timer_to_group_.erase(group_it);
                            
                            auto& group_timers = groups_[group_id];
                            group_timers.erase(
                                std::remove(group_timers.begin(), group_timers.end(), task.id),
                                group_timers.end()
                            );
                            
                            if (group_timers.empty()) {
                                groups_.erase(group_id);
                            }
                        }
                    }
                }
            }
        }
        
        // Execute expired tasks outside the lock to avoid deadlocks
        for (const auto& task : expired_tasks) {
            try {
                task.callback();
            } catch (const std::exception& e) {
                Logger::error("Exception in timer callback: {}", e.what());
            } catch (...) {
                Logger::error("Unknown exception in timer callback");
            }
        }
    }
}

TimerGroupId TimerManager::createGroup() {
    std::lock_guard<std::mutex> lock(mutex_);
    TimerGroupId id = next_group_id_++;
    if (id == 0) id = next_group_id_++; // Skip ID 0
    groups_[id] = {};
    return id;
}

bool TimerManager::addToGroup(TimerGroupId group_id, TimerId timer_id) {
    if (group_id == 0 || timer_id == 0) return false;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if timer exists
    if (tasks_.find(timer_id) == tasks_.end()) {
        return false;
    }
    
    // Check if group exists
    auto group_it = groups_.find(group_id);
    if (group_it == groups_.end()) {
        return false;
    }
    
    // Check if timer is already in another group
    auto timer_group_it = timer_to_group_.find(timer_id);
    if (timer_group_it != timer_to_group_.end()) {
        // If it's already in the same group, we're done
        if (timer_group_it->second == group_id) {
            return true;
        }
        
        // Remove from current group
        TimerGroupId old_group_id = timer_group_it->second;
        auto& old_group_timers = groups_[old_group_id];
        old_group_timers.erase(
            std::remove(old_group_timers.begin(), old_group_timers.end(), timer_id),
            old_group_timers.end()
        );
        
        if (old_group_timers.empty()) {
            groups_.erase(old_group_id);
        }
    }
    
    // Add to new group
    group_it->second.push_back(timer_id);
    timer_to_group_[timer_id] = group_id;
    
    return true;
}

bool TimerManager::removeFromGroup(TimerGroupId group_id, TimerId timer_id) {
    if (group_id == 0 || timer_id == 0) return false;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if timer is in the group
    auto timer_group_it = timer_to_group_.find(timer_id);
    if (timer_group_it == timer_to_group_.end() || timer_group_it->second != group_id) {
        return false;
    }
    
    // Remove from group
    auto group_it = groups_.find(group_id);
    if (group_it != groups_.end()) {
        auto& group_timers = group_it->second;
        group_timers.erase(
            std::remove(group_timers.begin(), group_timers.end(), timer_id),
            group_timers.end()
        );
        
        if (group_timers.empty()) {
            groups_.erase(group_id);
        }
    }
    
    timer_to_group_.erase(timer_group_it);
    
    return true;
}

bool TimerManager::cancelGroup(TimerGroupId group_id) {
    if (group_id == 0) return false;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto group_it = groups_.find(group_id);
    if (group_it == groups_.end()) {
        return false;
    }
    
    // Cancel all timers in the group
    for (TimerId timer_id : group_it->second) {
        tasks_.erase(timer_id);
        timer_to_group_.erase(timer_id);
    }
    
    groups_.erase(group_id);
    
    // Rebuild priority queue
    rebuildQueue();
    
    return true;
}

std::vector<TimerId> TimerManager::getGroupTimers(TimerGroupId group_id) const {
    if (group_id == 0) return {};
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = groups_.find(group_id);
    if (it == groups_.end()) {
        return {};
    }
    
    return it->second;
}

// Global timer functions
TimerId once(u64 delay_ms, std::function<void()> callback) {
    return TimerManager::instance().createOnce(delay_ms, std::move(callback));
}

TimerId repeat(u64 delay_ms, u64 interval_ms, std::function<void()> callback) {
    return TimerManager::instance().createRepeat(delay_ms, interval_ms, std::move(callback));
}

bool cancel(TimerId id) {
    return TimerManager::instance().cancel(id);
}

bool modify(TimerId id, u64 delay_ms, u64 interval_ms, bool repeat) {
    return TimerManager::instance().modify(id, delay_ms, interval_ms, repeat);
}

bool exists(TimerId id) {
    return TimerManager::instance().exists(id);
}

// Global timer group functions
TimerGroupId createTimerGroup() {
    return TimerManager::instance().createGroup();
}

bool addTimerToGroup(TimerGroupId group_id, TimerId timer_id) {
    return TimerManager::instance().addToGroup(group_id, timer_id);
}

bool removeTimerFromGroup(TimerGroupId group_id, TimerId timer_id) {
    return TimerManager::instance().removeFromGroup(group_id, timer_id);
}

bool cancelTimerGroup(TimerGroupId group_id) {
    return TimerManager::instance().cancelGroup(group_id);
}

std::vector<TimerId> getTimersInGroup(TimerGroupId group_id) {
    return TimerManager::instance().getGroupTimers(group_id);
}

} // namespace next_gen
