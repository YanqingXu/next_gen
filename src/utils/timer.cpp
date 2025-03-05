#include "../../include/utils/timer.h"
#include "../../include/utils/logger.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace next_gen {

// TimerManager implementation
TimerManager& TimerManager::instance() {
    static TimerManager instance;
    return instance;
}

void TimerManager::start() {
    if (running_) {
        return;
    }
    
    running_ = true;
    worker_thread_ = std::thread(&TimerManager::run, this);
    NEXT_GEN_LOG_INFO("Timer manager started");
}

void TimerManager::stop() {
    if (!running_) {
        return;
    }
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
    }
    
    cv_.notify_all();
    
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    
    NEXT_GEN_LOG_INFO("Timer manager stopped");
}

TimerId TimerManager::createOnce(u64 delay_ms, std::function<void()> callback) {
    return createTimer(delay_ms, 0, false, std::move(callback));
}

TimerId TimerManager::createRepeat(u64 delay_ms, u64 interval_ms, std::function<void()> callback) {
    return createTimer(delay_ms, interval_ms, true, std::move(callback));
}

bool TimerManager::cancel(TimerId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = tasks_.find(id);
    if (it == tasks_.end()) {
        return false;
    }
    
    // Remove from group if it belongs to one
    auto group_it = timer_to_group_.find(id);
    if (group_it != timer_to_group_.end()) {
        auto group_id = group_it->second;
        auto& timers = groups_[group_id];
        timers.erase(std::remove(timers.begin(), timers.end(), id), timers.end());
        timer_to_group_.erase(group_it);
    }
    
    tasks_.erase(it);
    rebuildQueue();
    
    return true;
}

bool TimerManager::modify(TimerId id, u64 delay_ms, u64 interval_ms, bool repeat) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = tasks_.find(id);
    if (it == tasks_.end()) {
        return false;
    }
    
    auto& task = it->second;
    task.next_run = getCurrentTimeMillis() + delay_ms;
    task.interval = interval_ms;
    task.repeat = repeat;
    
    rebuildQueue();
    cv_.notify_one();
    
    return true;
}

bool TimerManager::exists(TimerId id) const {
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
    timer_to_group_.clear();
    groups_.clear();
    
    while (!queue_.empty()) {
        queue_.pop();
    }
    
    NEXT_GEN_LOG_INFO("All timers cleared");
}

TimerId TimerManager::createTimer(u64 delay_ms, u64 interval_ms, bool repeat, std::function<void()> callback) {
    if (!callback) {
        NEXT_GEN_LOG_ERROR("Null timer callback");
        return 0;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    TimerId id = next_id_++;
    if (id == 0) {
        id = next_id_++;  // Skip 0 as it's used as invalid ID
    }
    
    TimerTask task;
    task.id = id;
    task.next_run = getCurrentTimeMillis() + delay_ms;
    task.interval = interval_ms;
    task.repeat = repeat;
    task.callback = std::move(callback);
    
    tasks_[id] = task;
    queue_.push(task);
    
    cv_.notify_one();
    
    return id;
}

void TimerManager::rebuildQueue() {
    // Clear the queue
    std::priority_queue<TimerTask, std::vector<TimerTask>, std::greater<TimerTask>> empty;
    std::swap(queue_, empty);
    
    // Rebuild from tasks map
    for (const auto& pair : tasks_) {
        queue_.push(pair.second);
    }
}

u64 TimerManager::getCurrentTimeMillis() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

void TimerManager::run() {
    NEXT_GEN_LOG_INFO("Timer worker thread started");
    
    while (running_) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (queue_.empty()) {
            // Wait until a timer is added or manager is stopped
            cv_.wait(lock, [this] { return !queue_.empty() || !running_; });
            
            if (!running_) {
                break;
            }
            
            continue;
        }
        
        // Get the next task to execute
        TimerTask task = queue_.top();
        
        // Calculate wait time
        u64 now = getCurrentTimeMillis();
        u64 wait_time = (task.next_run > now) ? (task.next_run - now) : 0;
        
        if (wait_time > 0) {
            // Wait until the next task is due or a new task is added or manager is stopped
            auto status = cv_.wait_for(lock, std::chrono::milliseconds(wait_time), 
                [this, &task] { 
                    return !running_ || queue_.empty() || queue_.top().id != task.id || queue_.top().next_run != task.next_run; 
                });
            
            if (!running_) {
                break;
            }
            
            if (status) {
                // Something changed, restart the loop
                continue;
            }
        }
        
        // Remove the task from the queue
        queue_.pop();
        
        // Check if the task still exists in the map
        auto it = tasks_.find(task.id);
        if (it == tasks_.end()) {
            continue;
        }
        
        // If task is repeating, update next run time and push back to queue
        if (task.repeat && task.interval > 0) {
            // Calculate the next run time based on the current time
            // This prevents drift when the system is under load
            task.next_run = now + task.interval;
            tasks_[task.id].next_run = task.next_run;
            queue_.push(task);
        } else {
            // Remove from group if it belongs to one
            auto group_it = timer_to_group_.find(task.id);
            if (group_it != timer_to_group_.end()) {
                auto group_id = group_it->second;
                auto& timers = groups_[group_id];
                timers.erase(std::remove(timers.begin(), timers.end(), task.id), timers.end());
                timer_to_group_.erase(group_it);
                
                // Remove the group if it's empty
                if (timers.empty()) {
                    groups_.erase(group_id);
                }
            }
            
            tasks_.erase(task.id);
        }
        
        // Execute the callback outside the lock
        auto callback = task.callback;
        lock.unlock();
        
        try {
            callback();
        } catch (const std::exception& e) {
            NEXT_GEN_LOG_ERROR("Exception in timer callback: " + std::string(e.what()));
        } catch (...) {
            NEXT_GEN_LOG_ERROR("Unknown exception in timer callback");
        }
    }
    
    NEXT_GEN_LOG_INFO("Timer worker thread stopped");
}

// Timer group functions
TimerGroupId TimerManager::createGroup() {
    std::lock_guard<std::mutex> lock(mutex_);
    TimerGroupId id = next_group_id_++;
    if (id == 0) {
        id = next_group_id_++;  // Skip 0 as it's used as invalid ID
    }
    groups_[id] = std::vector<TimerId>();
    return id;
}

bool TimerManager::addToGroup(TimerGroupId group_id, TimerId timer_id) {
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
    if (timer_group_it != timer_to_group_.end() && timer_group_it->second != group_id) {
        // Remove from the old group
        auto old_group_id = timer_group_it->second;
        auto& old_timers = groups_[old_group_id];
        old_timers.erase(std::remove(old_timers.begin(), old_timers.end(), timer_id), old_timers.end());
    }
    
    // Add to the new group
    timer_to_group_[timer_id] = group_id;
    group_it->second.push_back(timer_id);
    
    return true;
}

bool TimerManager::removeFromGroup(TimerGroupId group_id, TimerId timer_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if timer is in the specified group
    auto timer_group_it = timer_to_group_.find(timer_id);
    if (timer_group_it == timer_to_group_.end() || timer_group_it->second != group_id) {
        return false;
    }
    
    // Remove from group
    auto& timers = groups_[group_id];
    timers.erase(std::remove(timers.begin(), timers.end(), timer_id), timers.end());
    timer_to_group_.erase(timer_id);
    
    return true;
}

bool TimerManager::cancelGroup(TimerGroupId group_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if group exists
    auto group_it = groups_.find(group_id);
    if (group_it == groups_.end()) {
        return false;
    }
    
    // Cancel all timers in the group
    for (auto timer_id : group_it->second) {
        tasks_.erase(timer_id);
        timer_to_group_.erase(timer_id);
    }
    
    // Remove the group
    groups_.erase(group_id);
    
    // Rebuild the queue
    rebuildQueue();
    
    return true;
}

std::vector<TimerId> TimerManager::getGroupTimers(TimerGroupId group_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto group_it = groups_.find(group_id);
    if (group_it == groups_.end()) {
        return std::vector<TimerId>();
    }
    
    return group_it->second;
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
