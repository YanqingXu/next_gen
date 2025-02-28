#ifndef NEXT_GEN_TIMER_H
#define NEXT_GEN_TIMER_H

#include <functional>
#include <chrono>
#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <memory>
#include <unordered_map>
#include "../core/config.h"
#include "logger.h"

namespace next_gen {

// Timer ID type
using TimerId = u32;

// Timer task
struct TimerTask {
    TimerId id;                                // Timer ID
    u64 next_run;                              // Next run time (millisecond timestamp)
    u64 interval;                              // Interval time (milliseconds)
    bool repeat;                               // Whether to repeat
    std::function<void()> callback;            // Callback function
    
    // Comparison operator for priority queue
    bool operator>(const TimerTask& other) const {
        return next_run > other.next_run;
    }
};

// Timer manager
class NEXT_GEN_API TimerManager {
public:
    static TimerManager& instance() {
        static TimerManager instance;
        return instance;
    }
    
    // Start timer manager
    void start() {
        if (running_) {
            return;
        }
        
        running_ = true;
        worker_thread_ = std::thread(&TimerManager::run, this);
        NEXT_GEN_LOG_INFO("Timer manager started");
    }
    
    // Stop timer manager
    void stop() {
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
    
    // Create one-time timer
    TimerId createOnce(u64 delay_ms, std::function<void()> callback) {
        return createTimer(delay_ms, 0, false, std::move(callback));
    }
    
    // Create repeating timer
    TimerId createRepeat(u64 delay_ms, u64 interval_ms, std::function<void()> callback) {
        return createTimer(delay_ms, interval_ms, true, std::move(callback));
    }
    
    // Cancel timer
    bool cancel(TimerId id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = tasks_.find(id);
        if (it == tasks_.end()) {
            return false;
        }
        
        tasks_.erase(it);
        rebuildQueue();
        
        return true;
    }
    
    // Modify timer
    bool modify(TimerId id, u64 delay_ms, u64 interval_ms, bool repeat) {
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
    
    // Check if timer exists
    bool exists(TimerId id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return tasks_.find(id) != tasks_.end();
    }
    
    // Get current timer count
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return tasks_.size();
    }
    
    // Clear all timers
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        tasks_.clear();
        
        while (!queue_.empty()) {
            queue_.pop();
        }
        
        NEXT_GEN_LOG_INFO("All timers cleared");
    }
    
private:
    TimerManager() : running_(false), next_id_(1) {
        start();
    }
    
    ~TimerManager() {
        stop();
    }
    
    TimerManager(const TimerManager&) = delete;
    TimerManager& operator=(const TimerManager&) = delete;
    
    // Create timer
    TimerId createTimer(u64 delay_ms, u64 interval_ms, bool repeat, std::function<void()> callback) {
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
    
    // Rebuild priority queue
    void rebuildQueue() {
        // Clear the queue
        std::priority_queue<TimerTask, std::vector<TimerTask>, std::greater<TimerTask>> empty;
        std::swap(queue_, empty);
        
        // Rebuild from tasks map
        for (const auto& pair : tasks_) {
            queue_.push(pair.second);
        }
    }
    
    // Get current time in milliseconds
    u64 getCurrentTimeMillis() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    }
    
    // Worker thread function
    void run() {
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
                task.next_run = now + task.interval;
                tasks_[task.id].next_run = task.next_run;
                queue_.push(task);
            } else {
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
    
    std::atomic<bool> running_;
    std::thread worker_thread_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<TimerId> next_id_;
    std::unordered_map<TimerId, TimerTask> tasks_;
    std::priority_queue<TimerTask, std::vector<TimerTask>, std::greater<TimerTask>> queue_;
};

// Global timer functions
inline TimerId once(u64 delay_ms, std::function<void()> callback) {
    return TimerManager::instance().createOnce(delay_ms, std::move(callback));
}

// Create repeating timer
inline TimerId repeat(u64 delay_ms, u64 interval_ms, std::function<void()> callback) {
    return TimerManager::instance().createRepeat(delay_ms, interval_ms, std::move(callback));
}

// Cancel timer
inline bool cancel(TimerId id) {
    return TimerManager::instance().cancel(id);
}

// Modify timer
inline bool modify(TimerId id, u64 delay_ms, u64 interval_ms, bool repeat) {
    return TimerManager::instance().modify(id, delay_ms, interval_ms, repeat);
}

// Check if timer exists
inline bool exists(TimerId id) {
    return TimerManager::instance().exists(id);
}

} // namespace next_gen

#endif // NEXT_GEN_TIMER_H
