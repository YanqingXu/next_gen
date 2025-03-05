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
// Timer Group ID type
using TimerGroupId = u32;

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
    static TimerManager& instance();
    
    // Start timer manager
    void start();
    
    // Stop timer manager
    void stop();
    
    // Create one-time timer
    TimerId createOnce(u64 delay_ms, std::function<void()> callback);
    
    // Create repeating timer
    TimerId createRepeat(u64 delay_ms, u64 interval_ms, std::function<void()> callback);
    
    // Cancel timer
    bool cancel(TimerId id);
    
    // Modify timer
    bool modify(TimerId id, u64 delay_ms, u64 interval_ms, bool repeat);
    
    // Check if timer exists
    bool exists(TimerId id) const;
    
    // Get current timer count
    size_t size() const;
    
    // Clear all timers
    void clear();
    
    // Timer group functions
    TimerGroupId createGroup();
    bool addToGroup(TimerGroupId group_id, TimerId timer_id);
    bool removeFromGroup(TimerGroupId group_id, TimerId timer_id);
    bool cancelGroup(TimerGroupId group_id);
    std::vector<TimerId> getGroupTimers(TimerGroupId group_id) const;
    
private:
    TimerManager() : running_(false), next_id_(1), next_group_id_(1) {
        start();
    }
    
    ~TimerManager() {
        stop();
    }
    
    TimerManager(const TimerManager&) = delete;
    TimerManager& operator=(const TimerManager&) = delete;
    
    // Create timer
    TimerId createTimer(u64 delay_ms, u64 interval_ms, bool repeat, std::function<void()> callback);
    
    // Rebuild priority queue
    void rebuildQueue();
    
    // Get current time in milliseconds
    u64 getCurrentTimeMillis() const;
    
    // Worker thread function
    void run();
    
    std::atomic<bool> running_;
    std::thread worker_thread_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<TimerId> next_id_;
    std::atomic<TimerGroupId> next_group_id_;
    std::unordered_map<TimerId, TimerTask> tasks_;
    std::priority_queue<TimerTask, std::vector<TimerTask>, std::greater<TimerTask>> queue_;
    
    // Timer group management
    std::unordered_map<TimerGroupId, std::vector<TimerId>> groups_;
    std::unordered_map<TimerId, TimerGroupId> timer_to_group_;
};

// Global timer functions
NEXT_GEN_API TimerId once(u64 delay_ms, std::function<void()> callback);
NEXT_GEN_API TimerId repeat(u64 delay_ms, u64 interval_ms, std::function<void()> callback);
NEXT_GEN_API bool cancel(TimerId id);
NEXT_GEN_API bool modify(TimerId id, u64 delay_ms, u64 interval_ms, bool repeat);
NEXT_GEN_API bool exists(TimerId id);

// Global timer group functions
NEXT_GEN_API TimerGroupId createTimerGroup();
NEXT_GEN_API bool addTimerToGroup(TimerGroupId group_id, TimerId timer_id);
NEXT_GEN_API bool removeTimerFromGroup(TimerGroupId group_id, TimerId timer_id);
NEXT_GEN_API bool cancelTimerGroup(TimerGroupId group_id);
NEXT_GEN_API std::vector<TimerId> getTimersInGroup(TimerGroupId group_id);

} // namespace next_gen

#endif // NEXT_GEN_TIMER_H
