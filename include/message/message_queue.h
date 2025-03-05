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
    void push(std::unique_ptr<Message> message) override;
    
    // Pop message from queue, block if queue is empty
    std::unique_ptr<Message> pop() override;
    
    // Try to pop message from queue, return nullptr if queue is empty
    std::unique_ptr<Message> tryPop() override;
    
    // Try to pop message from queue, wait for specified time if queue is empty
    std::unique_ptr<Message> waitAndPop(const std::chrono::milliseconds& timeout) override;
    
    // Get queue size
    size_t size() const override;
    
    // Check if queue is empty
    bool empty() const override;
    
    // Clear queue
    void clear() override;
    
    // Shutdown queue
    void shutdown() override;
    
    // Check if queue is shutdown
    bool isShutdown() const override;
    
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
    
    ~PriorityMessageQueue();
    
    // Push message to queue
    void push(std::unique_ptr<Message> message) override;
    
    // Pop message from queue, block if queue is empty
    std::unique_ptr<Message> pop() override;
    
    // Try to pop message from queue, return nullptr if queue is empty
    std::unique_ptr<Message> tryPop() override;
    
    // Try to pop message from queue, wait for specified time if queue is empty
    std::unique_ptr<Message> waitAndPop(const std::chrono::milliseconds& timeout) override;
    
    // Get queue size
    size_t size() const override;
    
    // Check if queue is empty
    bool empty() const override;
    
    // Clear queue
    void clear() override;
    
    // Shutdown queue
    void shutdown() override;
    
    // Check if queue is shutdown
    bool isShutdown() const override;
    
protected:
    // Calculate message priority (can be overridden in subclasses)
    virtual int calculatePriority(const Message& message) const;
    
private:
    // Priority comparator
    struct PriorityCompare {
        bool operator()(
            const std::pair<int, std::unique_ptr<Message>>& a,
            const std::pair<int, std::unique_ptr<Message>>& b
        ) const;
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

// Lock-free message queue implementation using atomic operations
class NEXT_GEN_API LockFreeMessageQueue : public MessageQueue {
public:
    LockFreeMessageQueue(size_t capacity = 1024)
        : capacity_(capacity), head_(0), tail_(0), shutdown_(false) {
        // Allocate buffer with capacity + 1 elements (to distinguish between empty and full)
        buffer_ = new std::atomic<Message*>[capacity + 1];
        for (size_t i = 0; i <= capacity; ++i) {
            buffer_[i].store(nullptr, std::memory_order_relaxed);
        }
    }
    
    ~LockFreeMessageQueue();
    
    // Push message to queue
    void push(std::unique_ptr<Message> message) override;
    
    // Pop message from queue, block if queue is empty
    std::unique_ptr<Message> pop() override;
    
    // Try to pop message from queue, return nullptr if queue is empty
    std::unique_ptr<Message> tryPop();
    
    // Try to pop message from queue, wait for specified time if queue is empty
    std::unique_ptr<Message> waitAndPop(const std::chrono::milliseconds& timeout) override;
    
    // Get queue size (approximate)
    size_t size() const override;
    
    // Check if queue is empty
    bool empty() const override;
    
    // Clear queue
    void clear() override;
    
    // Shutdown queue
    void shutdown() override;
    
    // Check if queue is shutdown
    bool isShutdown() const override;
    
private:
    size_t capacity_;
    std::atomic<Message*>* buffer_;
    std::atomic<size_t> head_;
    std::atomic<size_t> tail_;
    std::atomic<bool> shutdown_;
};

// Multi-producer, multi-consumer lock-free message queue using ring buffer
class NEXT_GEN_API MPMCMessageQueue : public MessageQueue {
public:
    MPMCMessageQueue(size_t capacity = 1024)
        : capacity_(capacity), shutdown_(false) {
        // Initialize ring buffer
        buffer_ = new Cell[capacity_];
        for (size_t i = 0; i < capacity_; ++i) {
            buffer_[i].sequence.store(i, std::memory_order_relaxed);
        }
        
        // Initialize enqueue and dequeue positions
        enqueue_pos_.store(0, std::memory_order_relaxed);
        dequeue_pos_.store(0, std::memory_order_relaxed);
    }
    
    ~MPMCMessageQueue();
    
    // Push message to queue
    void push(std::unique_ptr<Message> message) override;
    
    // Pop message from queue, block if queue is empty
    std::unique_ptr<Message> pop();
    
    // Try to pop message from queue, return nullptr if queue is empty
    std::unique_ptr<Message> tryPop() override;
    
    // Try to pop message from queue, wait for specified time if queue is empty
    std::unique_ptr<Message> waitAndPop(const std::chrono::milliseconds& timeout) override;

    // Get queue size
    size_t size() const;
    
    // Check if queue is empty
    bool empty() const override;
    
    // Clear queue
    void clear() override;
    
    // Shutdown queue
    void shutdown() override;
    
    // Check if queue is shutdown
    bool isShutdown() const override;
    
private:
    struct Cell {
        std::atomic<size_t> sequence;
        Message* data;
        
        Cell() : data(nullptr) {
            sequence.store(0, std::memory_order_relaxed);
        }
    };
    
    size_t capacity_;
    Cell* buffer_;
    std::atomic<size_t> enqueue_pos_;
    std::atomic<size_t> dequeue_pos_;
    std::atomic<bool> shutdown_;
    
    // Cache line padding to prevent false sharing
    char padding_[64 - sizeof(std::atomic<bool>)];
};

// Factory function to create message queue
std::unique_ptr<MessageQueue> createMessageQueue(const std::string& type = "default", size_t capacity = 1024);

} // namespace next_gen

#endif // NEXT_GEN_MESSAGE_QUEUE_H
