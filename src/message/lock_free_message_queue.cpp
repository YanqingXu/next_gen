#include "../../include/message/message_queue.h"
#include <thread>
#include <chrono>

namespace next_gen {

LockFreeMessageQueue::~LockFreeMessageQueue() {
    shutdown();
    
    // Clean up any remaining messages
    clear();
    
    delete[] buffer_;
}

void LockFreeMessageQueue::push(std::unique_ptr<Message> message) {
    if (isShutdown()) {
        Logger::warning("Message dropped: queue is shutdown");
        return;
    }
    
    Message* msg = message.release();
    
    size_t currentTail = tail_.load(std::memory_order_relaxed);
    size_t nextTail = (currentTail + 1) % (capacity_ + 1);
    
    while (nextTail == head_.load(std::memory_order_acquire)) {
        // Queue is full, yield and try again
        if (isShutdown()) {
            delete msg;
            return;
        }
        std::this_thread::yield();
        currentTail = tail_.load(std::memory_order_relaxed);
        nextTail = (currentTail + 1) % (capacity_ + 1);
    }
    
    buffer_[currentTail].store(msg, std::memory_order_release);
    tail_.store(nextTail, std::memory_order_release);
}

std::unique_ptr<Message> LockFreeMessageQueue::pop() {
    while (!isShutdown()) {
        std::unique_ptr<Message> result = tryPop();
        if (result) {
            return result;
        }
        std::this_thread::yield();
    }
    return nullptr;
}

std::unique_ptr<Message> LockFreeMessageQueue::tryPop() {
    size_t currentHead = head_.load(std::memory_order_relaxed);
    
    if (currentHead == tail_.load(std::memory_order_acquire)) {
        // Queue is empty
        return nullptr;
    }
    
    Message* msg = buffer_[currentHead].load(std::memory_order_relaxed);
    size_t nextHead = (currentHead + 1) % (capacity_ + 1);
    
    if (!head_.compare_exchange_strong(currentHead, nextHead,
                                      std::memory_order_release,
                                      std::memory_order_relaxed)) {
        // Another thread beat us to it
        return nullptr;
    }
    
    buffer_[currentHead].store(nullptr, std::memory_order_relaxed);
    
    return std::unique_ptr<Message>(msg);
}

std::unique_ptr<Message> LockFreeMessageQueue::waitAndPop(const std::chrono::milliseconds& timeout) {
    auto endTime = std::chrono::steady_clock::now() + timeout;
    
    while (std::chrono::steady_clock::now() < endTime && !isShutdown()) {
        std::unique_ptr<Message> result = tryPop();
        if (result) {
            return result;
        }
        std::this_thread::yield();
    }
    return nullptr;
}

size_t LockFreeMessageQueue::size() const {
    size_t head = head_.load(std::memory_order_acquire);
    size_t tail = tail_.load(std::memory_order_acquire);
    
    if (tail >= head) {
        return tail - head;
    } else {
        return (capacity_ + 1) - (head - tail);
    }
}

bool LockFreeMessageQueue::empty() const {
    return head_.load(std::memory_order_acquire) == 
           tail_.load(std::memory_order_acquire);
}

void LockFreeMessageQueue::clear() {
    while (!empty()) {
        std::unique_ptr<Message> msg = tryPop();
        // Message will be automatically deleted when unique_ptr goes out of scope
    }
}

void LockFreeMessageQueue::shutdown() {
    shutdown_.store(true, std::memory_order_release);
}

bool LockFreeMessageQueue::isShutdown() const {
    return shutdown_.load(std::memory_order_acquire);
}

} // namespace next_gen
