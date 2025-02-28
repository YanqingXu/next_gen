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

// Message queue interface
class NEXT_GEN_API MessageQueue {
public:
    virtual ~MessageQueue() = default;
    
    // Push message to queue
    virtual void push(std::unique_ptr<Message> message) = 0;
    
    // Pop message from queue, block if queue is empty
    virtual std::unique_ptr<Message> pop() = 0;
    
    // Try to pop message from queue, return nullptr if queue is empty
    virtual std::unique_ptr<Message> tryPop() = 0;
    
    // Try to pop message from queue, wait for specified time if queue is empty
    virtual std::unique_ptr<Message> waitAndPop(const std::chrono::milliseconds& timeout) = 0;
    
    // Get queue size
    virtual size_t size() const = 0;
    
    // Check if queue is empty
    virtual bool empty() const = 0;
    
    // Clear queue
    virtual void clear() = 0;
    
    // Shutdown queue
    virtual void shutdown() = 0;
    
    // Check if queue is shutdown
    virtual bool isShutdown() const = 0;
};

// Default message queue implementation
class NEXT_GEN_API DefaultMessageQueue : public MessageQueue {
public:
    DefaultMessageQueue(size_t maxSize = 0) 
        : max_size_(maxSize), shutdown_(false) {}
    
    ~DefaultMessageQueue() {
        shutdown();
    }
    
    // Push message to queue
    void push(std::unique_ptr<Message> message) override {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // If queue is shutdown, discard message
        if (shutdown_) {
            NEXT_GEN_LOG_WARNING("Attempt to push message to shutdown queue");
            return;
        }
        
        // If queue is full, wait
        if (max_size_ > 0 && queue_.size() >= max_size_) {
            NEXT_GEN_LOG_WARNING("Message queue is full, waiting for space");
            not_full_.wait(lock, [this] { 
                return queue_.size() < max_size_ || shutdown_; 
            });
            
            // Check again if queue is shutdown
            if (shutdown_) {
                NEXT_GEN_LOG_WARNING("Queue shutdown while waiting to push message");
                return;
            }
        }
        
        // Push message
        queue_.push(std::move(message));
        
        // Notify waiting consumers
        lock.unlock();
        not_empty_.notify_one();
    }
    
    // Pop message from queue, block if queue is empty
    std::unique_ptr<Message> pop() override {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // Wait for queue to be non-empty or shutdown
        not_empty_.wait(lock, [this] { 
            return !queue_.empty() || shutdown_; 
        });
        
        // If queue is shutdown and empty, return nullptr
        if (queue_.empty()) {
            return nullptr;
        }
        
        // Pop message
        std::unique_ptr<Message> message = std::move(queue_.front());
        queue_.pop();
        
        // Notify waiting producers
        lock.unlock();
        not_full_.notify_one();
        
        return message;
    }
    
    // Try to pop message from queue, return nullptr if queue is empty
    std::unique_ptr<Message> tryPop() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // If queue is empty, return nullptr
        if (queue_.empty()) {
            return nullptr;
        }
        
        // Pop message
        std::unique_ptr<Message> message = std::move(queue_.front());
        queue_.pop();
        
        // Notify waiting producers
        not_full_.notify_one();
        
        return message;
    }
    
    // Try to pop message from queue, wait for specified time if queue is empty
    std::unique_ptr<Message> waitAndPop(const std::chrono::milliseconds& timeout) override {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // Wait for queue to be non-empty or timeout
        bool success = not_empty_.wait_for(lock, timeout, [this] { 
            return !queue_.empty() || shutdown_; 
        });
        
        // If timeout or queue is empty, return nullptr
        if (!success || queue_.empty()) {
            return nullptr;
        }
        
        // Pop message
        std::unique_ptr<Message> message = std::move(queue_.front());
        queue_.pop();
        
        // Notify waiting producers
        not_full_.notify_one();
        
        return message;
    }
    
    // Get queue size
    size_t size() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
    
    // Check if queue is empty
    bool empty() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }
    
    // Clear queue
    void clear() override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::queue<std::unique_ptr<Message>> empty;
        std::swap(queue_, empty);
        not_full_.notify_all();
    }
    
    // Shutdown queue
    void shutdown() override {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = true;
        not_empty_.notify_all();
        not_full_.notify_all();
    }
    
    // Check if queue is shutdown
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

// Priority message queue implementation
class NEXT_GEN_API PriorityMessageQueue : public MessageQueue {
public:
    PriorityMessageQueue(size_t maxSize = 0) 
        : max_size_(maxSize), shutdown_(false) {}
    
    ~PriorityMessageQueue() {
        shutdown();
    }
    
    // Push message to queue
    void push(std::unique_ptr<Message> message) override {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // If queue is shutdown, discard message
        if (shutdown_) {
            NEXT_GEN_LOG_WARNING("Attempt to push message to shutdown queue");
            return;
        }
        
        // If queue is full, wait
        if (max_size_ > 0 && queue_.size() >= max_size_) {
            NEXT_GEN_LOG_WARNING("Message queue is full, waiting for space");
            not_full_.wait(lock, [this] { 
                return queue_.size() < max_size_ || shutdown_; 
            });
            
            // Check again if queue is shutdown
            if (shutdown_) {
                NEXT_GEN_LOG_WARNING("Queue shutdown while waiting to push message");
                return;
            }
        }
        
        // Calculate priority (can be adjusted based on message type or other factors)
        int priority = calculatePriority(*message);
        
        // Push message
        queue_.push(std::make_pair(priority, std::move(message)));
        
        // Notify waiting consumers
        lock.unlock();
        not_empty_.notify_one();
    }
    
    // Pop message from queue, block if queue is empty
    std::unique_ptr<Message> pop() override {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // Wait for queue to be non-empty or shutdown
        not_empty_.wait(lock, [this] { 
            return !queue_.empty() || shutdown_; 
        });
        
        // If queue is shutdown and empty, return nullptr
        if (queue_.empty()) {
            return nullptr;
        }
        
        // Pop message
        auto top = std::move(const_cast<std::pair<int, std::unique_ptr<Message>>&>(queue_.top()));
        queue_.pop();
        std::unique_ptr<Message> message = std::move(top.second);
        
        // Notify waiting producers
        lock.unlock();
        not_full_.notify_one();
        
        return message;
    }
    
    // Try to pop message from queue, return nullptr if queue is empty
    std::unique_ptr<Message> tryPop() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // If queue is empty, return nullptr
        if (queue_.empty()) {
            return nullptr;
        }
        
        // Pop message
        auto top = std::move(const_cast<std::pair<int, std::unique_ptr<Message>>&>(queue_.top()));
        queue_.pop();
        std::unique_ptr<Message> message = std::move(top.second);
        
        // Notify waiting producers
        not_full_.notify_one();
        
        return message;
    }
    
    // Try to pop message from queue, wait for specified time if queue is empty
    std::unique_ptr<Message> waitAndPop(const std::chrono::milliseconds& timeout) override {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // Wait for queue to be non-empty or timeout
        bool success = not_empty_.wait_for(lock, timeout, [this] { 
            return !queue_.empty() || shutdown_; 
        });
        
        // If timeout or queue is empty, return nullptr
        if (!success || queue_.empty()) {
            return nullptr;
        }
        
        // Pop message
        auto top = std::move(const_cast<std::pair<int, std::unique_ptr<Message>>&>(queue_.top()));
        queue_.pop();
        std::unique_ptr<Message> message = std::move(top.second);
        
        // Notify waiting producers
        not_full_.notify_one();
        
        return message;
    }
    
    // Get queue size
    size_t size() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
    
    // Check if queue is empty
    bool empty() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }
    
    // Clear queue
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
    
    // Shutdown queue
    void shutdown() override {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = true;
        not_empty_.notify_all();
        not_full_.notify_all();
    }
    
    // Check if queue is shutdown
    bool isShutdown() const override {
        return shutdown_;
    }
    
protected:
    // Calculate message priority (can be overridden in subclasses)
    virtual int calculatePriority(const Message& message) const {
        // Default implementation: calculate priority based on message category
        // Priority calculation can be adjusted as needed
        return static_cast<int>(message.getCategory());
    }
    
private:
    // Priority comparator
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
