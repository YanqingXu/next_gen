#ifndef NEXT_GEN_MESSAGE_QUEUE_H
#define NEXT_GEN_MESSAGE_QUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <memory>
#include <atomic>
#include "message.h"
#include "../utils/logger.h"

namespace next_gen {

// 消息队列接口
class NEXT_GEN_API MessageQueue {
public:
    virtual ~MessageQueue() = default;
    
    // 推送消息到队列
    virtual void push(std::unique_ptr<Message> message) = 0;
    
    // 从队列中弹出消息，如果队列为空则阻塞
    virtual std::unique_ptr<Message> pop() = 0;
    
    // 尝试从队列中弹出消息，如果队列为空则返回nullptr
    virtual std::unique_ptr<Message> tryPop() = 0;
    
    // 尝试从队列中弹出消息，如果队列为空则等待指定时间
    virtual std::unique_ptr<Message> waitAndPop(const std::chrono::milliseconds& timeout) = 0;
    
    // 获取队列大小
    virtual size_t size() const = 0;
    
    // 检查队列是否为空
    virtual bool empty() const = 0;
    
    // 清空队列
    virtual void clear() = 0;
    
    // 关闭队列
    virtual void shutdown() = 0;
    
    // 检查队列是否已关闭
    virtual bool isShutdown() const = 0;
};

// 默认消息队列实现
class NEXT_GEN_API DefaultMessageQueue : public MessageQueue {
public:
    DefaultMessageQueue(size_t maxSize = 0) 
        : max_size_(maxSize), shutdown_(false) {}
    
    ~DefaultMessageQueue() {
        shutdown();
    }
    
    // 推送消息到队列
    void push(std::unique_ptr<Message> message) override {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // 如果队列已关闭，则丢弃消息
        if (shutdown_) {
            NEXT_GEN_LOG_WARNING("Attempt to push message to shutdown queue");
            return;
        }
        
        // 如果队列已满，则等待
        if (max_size_ > 0 && queue_.size() >= max_size_) {
            NEXT_GEN_LOG_WARNING("Message queue is full, waiting for space");
            not_full_.wait(lock, [this] { 
                return queue_.size() < max_size_ || shutdown_; 
            });
            
            // 再次检查队列是否已关闭
            if (shutdown_) {
                NEXT_GEN_LOG_WARNING("Queue shutdown while waiting to push message");
                return;
            }
        }
        
        // 推送消息
        queue_.push(std::move(message));
        
        // 通知等待的消费者
        lock.unlock();
        not_empty_.notify_one();
    }
    
    // 从队列中弹出消息，如果队列为空则阻塞
    std::unique_ptr<Message> pop() override {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // 等待队列非空或关闭
        not_empty_.wait(lock, [this] { 
            return !queue_.empty() || shutdown_; 
        });
        
        // 如果队列已关闭且为空，则返回nullptr
        if (queue_.empty()) {
            return nullptr;
        }
        
        // 弹出消息
        std::unique_ptr<Message> message = std::move(queue_.front());
        queue_.pop();
        
        // 通知等待的生产者
        lock.unlock();
        not_full_.notify_one();
        
        return message;
    }
    
    // 尝试从队列中弹出消息，如果队列为空则返回nullptr
    std::unique_ptr<Message> tryPop() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // 如果队列为空，则返回nullptr
        if (queue_.empty()) {
            return nullptr;
        }
        
        // 弹出消息
        std::unique_ptr<Message> message = std::move(queue_.front());
        queue_.pop();
        
        // 通知等待的生产者
        not_full_.notify_one();
        
        return message;
    }
    
    // 尝试从队列中弹出消息，如果队列为空则等待指定时间
    std::unique_ptr<Message> waitAndPop(const std::chrono::milliseconds& timeout) override {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // 等待队列非空或超时
        bool success = not_empty_.wait_for(lock, timeout, [this] { 
            return !queue_.empty() || shutdown_; 
        });
        
        // 如果超时或队列为空，则返回nullptr
        if (!success || queue_.empty()) {
            return nullptr;
        }
        
        // 弹出消息
        std::unique_ptr<Message> message = std::move(queue_.front());
        queue_.pop();
        
        // 通知等待的生产者
        not_full_.notify_one();
        
        return message;
    }
    
    // 获取队列大小
    size_t size() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
    
    // 检查队列是否为空
    bool empty() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }
    
    // 清空队列
    void clear() override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::queue<std::unique_ptr<Message>> empty;
        std::swap(queue_, empty);
        not_full_.notify_all();
    }
    
    // 关闭队列
    void shutdown() override {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = true;
        not_empty_.notify_all();
        not_full_.notify_all();
    }
    
    // 检查队列是否已关闭
    bool isShutdown() const override {
        return shutdown_;
    }
    
private:
    std::queue<std::unique_ptr<Message>> queue_;
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    size_t max_size_;
    std::atomic<bool> shutdown_;
};

// 优先级消息队列实现
class NEXT_GEN_API PriorityMessageQueue : public MessageQueue {
public:
    PriorityMessageQueue(size_t maxSize = 0) 
        : max_size_(maxSize), shutdown_(false) {}
    
    ~PriorityMessageQueue() {
        shutdown();
    }
    
    // 推送消息到队列
    void push(std::unique_ptr<Message> message) override {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // 如果队列已关闭，则丢弃消息
        if (shutdown_) {
            NEXT_GEN_LOG_WARNING("Attempt to push message to shutdown queue");
            return;
        }
        
        // 如果队列已满，则等待
        if (max_size_ > 0 && queue_.size() >= max_size_) {
            NEXT_GEN_LOG_WARNING("Message queue is full, waiting for space");
            not_full_.wait(lock, [this] { 
                return queue_.size() < max_size_ || shutdown_; 
            });
            
            // 再次检查队列是否已关闭
            if (shutdown_) {
                NEXT_GEN_LOG_WARNING("Queue shutdown while waiting to push message");
                return;
            }
        }
        
        // 计算优先级 (可以根据消息类型或其他因素调整)
        int priority = calculatePriority(*message);
        
        // 推送消息
        queue_.push(std::make_pair(priority, std::move(message)));
        
        // 通知等待的消费者
        lock.unlock();
        not_empty_.notify_one();
    }
    
    // 从队列中弹出消息，如果队列为空则阻塞
    std::unique_ptr<Message> pop() override {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // 等待队列非空或关闭
        not_empty_.wait(lock, [this] { 
            return !queue_.empty() || shutdown_; 
        });
        
        // 如果队列已关闭且为空，则返回nullptr
        if (queue_.empty()) {
            return nullptr;
        }
        
        // 弹出消息
        std::unique_ptr<Message> message = std::move(queue_.top().second);
        queue_.pop();
        
        // 通知等待的生产者
        lock.unlock();
        not_full_.notify_one();
        
        return message;
    }
    
    // 尝试从队列中弹出消息，如果队列为空则返回nullptr
    std::unique_ptr<Message> tryPop() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // 如果队列为空，则返回nullptr
        if (queue_.empty()) {
            return nullptr;
        }
        
        // 弹出消息
        std::unique_ptr<Message> message = std::move(queue_.top().second);
        queue_.pop();
        
        // 通知等待的生产者
        not_full_.notify_one();
        
        return message;
    }
    
    // 尝试从队列中弹出消息，如果队列为空则等待指定时间
    std::unique_ptr<Message> waitAndPop(const std::chrono::milliseconds& timeout) override {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // 等待队列非空或超时
        bool success = not_empty_.wait_for(lock, timeout, [this] { 
            return !queue_.empty() || shutdown_; 
        });
        
        // 如果超时或队列为空，则返回nullptr
        if (!success || queue_.empty()) {
            return nullptr;
        }
        
        // 弹出消息
        std::unique_ptr<Message> message = std::move(queue_.top().second);
        queue_.pop();
        
        // 通知等待的生产者
        not_full_.notify_one();
        
        return message;
    }
    
    // 获取队列大小
    size_t size() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
    
    // 检查队列是否为空
    bool empty() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }
    
    // 清空队列
    void clear() override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::priority_queue<
            std::pair<int, std::unique_ptr<Message>>,
            std::vector<std::pair<int, std::unique_ptr<Message>>>,
            PriorityCompare
        > empty;
        std::swap(queue_, empty);
        not_full_.notify_all();
    }
    
    // 关闭队列
    void shutdown() override {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = true;
        not_empty_.notify_all();
        not_full_.notify_all();
    }
    
    // 检查队列是否已关闭
    bool isShutdown() const override {
        return shutdown_;
    }
    
protected:
    // 计算消息优先级 (可以在子类中重写)
    virtual int calculatePriority(const Message& message) const {
        // 默认实现：根据消息类别计算优先级
        // 可以根据需要调整优先级计算方式
        return static_cast<int>(message.getCategory());
    }
    
private:
    // 优先级比较器
    struct PriorityCompare {
        bool operator()(
            const std::pair<int, std::unique_ptr<Message>>& a,
            const std::pair<int, std::unique_ptr<Message>>& b
        ) const {
            return a.first < b.first;
        }
    };
    
    std::priority_queue<
        std::pair<int, std::unique_ptr<Message>>,
        std::vector<std::pair<int, std::unique_ptr<Message>>>,
        PriorityCompare
    > queue_;
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    size_t max_size_;
    std::atomic<bool> shutdown_;
};

} // namespace next_gen

#endif // NEXT_GEN_MESSAGE_QUEUE_H
