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

// 定时器ID类型
using TimerId = u32;

// 定时器任务
struct TimerTask {
    TimerId id;                                // 定时器ID
    u64 next_run;                              // 下次运行时间（毫秒时间戳）
    u64 interval;                              // 间隔时间（毫秒）
    bool repeat;                               // 是否重复
    std::function<void()> callback;            // 回调函数
    
    // 比较运算符，用于优先队列
    bool operator>(const TimerTask& other) const {
        return next_run > other.next_run;
    }
};

// 定时器管理器
class NEXT_GEN_API TimerManager {
public:
    static TimerManager& instance() {
        static TimerManager instance;
        return instance;
    }
    
    // 启动定时器管理器
    void start() {
        if (running_) {
            return;
        }
        
        running_ = true;
        worker_thread_ = std::thread(&TimerManager::run, this);
        NEXT_GEN_LOG_INFO("Timer manager started");
    }
    
    // 停止定时器管理器
    void stop() {
        if (!running_) {
            return;
        }
        
        running_ = false;
        cv_.notify_all();
        
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
        
        NEXT_GEN_LOG_INFO("Timer manager stopped");
    }
    
    // 创建一次性定时器
    TimerId createOnce(u64 delay_ms, std::function<void()> callback) {
        return createTimer(delay_ms, 0, false, std::move(callback));
    }
    
    // 创建重复定时器
    TimerId createRepeat(u64 delay_ms, u64 interval_ms, std::function<void()> callback) {
        return createTimer(delay_ms, interval_ms, true, std::move(callback));
    }
    
    // 取消定时器
    bool cancel(TimerId id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = timers_.find(id);
        if (it == timers_.end()) {
            return false;
        }
        
        timers_.erase(it);
        return true;
    }
    
    // 修改定时器
    bool modify(TimerId id, u64 delay_ms, u64 interval_ms, bool repeat) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = timers_.find(id);
        if (it == timers_.end()) {
            return false;
        }
        
        // 更新定时器参数
        auto now = getCurrentTimeMillis();
        it->second.next_run = now + delay_ms;
        it->second.interval = interval_ms;
        it->second.repeat = repeat;
        
        // 重新调整优先队列
        rebuildQueue();
        cv_.notify_one();
        
        return true;
    }
    
    // 检查定时器是否存在
    bool exists(TimerId id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return timers_.find(id) != timers_.end();
    }
    
    // 获取当前定时器数量
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return timers_.size();
    }
    
    // 清空所有定时器
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        timers_.clear();
        
        // 清空优先队列
        std::priority_queue<TimerTask, std::vector<TimerTask>, std::greater<>> empty;
        std::swap(queue_, empty);
        
        cv_.notify_one();
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
    
    // 创建定时器
    TimerId createTimer(u64 delay_ms, u64 interval_ms, bool repeat, std::function<void()> callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // 生成定时器ID
        TimerId id = next_id_++;
        
        // 创建定时器任务
        TimerTask task;
        task.id = id;
        task.next_run = getCurrentTimeMillis() + delay_ms;
        task.interval = interval_ms;
        task.repeat = repeat;
        task.callback = std::move(callback);
        
        // 添加到定时器映射和优先队列
        timers_[id] = task;
        queue_.push(task);
        
        // 通知工作线程
        cv_.notify_one();
        
        return id;
    }
    
    // 重建优先队列
    void rebuildQueue() {
        std::priority_queue<TimerTask, std::vector<TimerTask>, std::greater<>> new_queue;
        
        for (const auto& pair : timers_) {
            new_queue.push(pair.second);
        }
        
        std::swap(queue_, new_queue);
    }
    
    // 获取当前时间（毫秒）
    static u64 getCurrentTimeMillis() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    
    // 工作线程函数
    void run() {
        while (running_) {
            std::unique_lock<std::mutex> lock(mutex_);
            
            if (queue_.empty()) {
                // 如果队列为空，等待新的定时器
                cv_.wait(lock, [this] { return !running_ || !queue_.empty(); });
                if (!running_) {
                    break;
                }
            } else {
                // 获取下一个要执行的定时器
                const TimerTask& task = queue_.top();
                
                // 计算等待时间
                auto now = getCurrentTimeMillis();
                auto wait_time = (task.next_run > now) ? (task.next_run - now) : 0;
                
                if (wait_time > 0) {
                    // 等待直到下一个定时器到期或被通知
                    cv_.wait_for(lock, std::chrono::milliseconds(wait_time), 
                                [this, &task, now] { 
                                    return !running_ || queue_.empty() || queue_.top().next_run != task.next_run; 
                                });
                    if (!running_) {
                        break;
                    }
                    continue;
                }
                
                // 取出定时器任务
                TimerTask current_task = queue_.top();
                queue_.pop();
                
                // 从映射中移除（如果是一次性定时器）
                if (!current_task.repeat) {
                    timers_.erase(current_task.id);
                } else {
                    // 更新下次运行时间并重新入队
                    current_task.next_run = now + current_task.interval;
                    timers_[current_task.id].next_run = current_task.next_run;
                    queue_.push(current_task);
                }
                
                // 解锁互斥锁，执行回调
                lock.unlock();
                
                try {
                    current_task.callback();
                } catch (const std::exception& e) {
                    NEXT_GEN_LOG_ERROR("Exception in timer callback: " + std::string(e.what()));
                } catch (...) {
                    NEXT_GEN_LOG_ERROR("Unknown exception in timer callback");
                }
            }
        }
    }
    
    std::atomic<bool> running_;
    std::thread worker_thread_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::priority_queue<TimerTask, std::vector<TimerTask>, std::greater<>> queue_;
    std::unordered_map<TimerId, TimerTask> timers_;
    std::atomic<TimerId> next_id_;
};

// 定时器类
class NEXT_GEN_API Timer {
public:
    // 创建一次性定时器
    static TimerId once(u64 delay_ms, std::function<void()> callback) {
        return TimerManager::instance().createOnce(delay_ms, std::move(callback));
    }
    
    // 创建重复定时器
    static TimerId repeat(u64 delay_ms, u64 interval_ms, std::function<void()> callback) {
        return TimerManager::instance().createRepeat(delay_ms, interval_ms, std::move(callback));
    }
    
    // 取消定时器
    static bool cancel(TimerId id) {
        return TimerManager::instance().cancel(id);
    }
    
    // 修改定时器
    static bool modify(TimerId id, u64 delay_ms, u64 interval_ms, bool repeat) {
        return TimerManager::instance().modify(id, delay_ms, interval_ms, repeat);
    }
    
    // 检查定时器是否存在
    static bool exists(TimerId id) {
        return TimerManager::instance().exists(id);
    }
};

} // namespace next_gen

#endif // NEXT_GEN_TIMER_H
