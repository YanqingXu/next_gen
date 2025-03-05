#include "../../include/message/message_queue.h"
#include "../../include/utils/logger.h"

namespace next_gen {

// DefaultMessageQueue implementation

void DefaultMessageQueue::push(std::unique_ptr<Message> message) {
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

std::unique_ptr<Message> DefaultMessageQueue::pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    
    // Wait for queue to be non-empty or shutdown
    not_empty_.wait(lock, [this] { 
        return !queue_.empty() || shutdown_; 
    });
    
    // If queue is shutdown and empty, return nullptr
    if (queue_.empty()) {
        return nullptr;
    }
    
    // Get message
    std::unique_ptr<Message> message = std::move(queue_.front());
    queue_.pop();
    
    // Notify waiting producers
    lock.unlock();
    if (max_size_ > 0) {
        not_full_.notify_one();
    }
    
    return message;
}

std::unique_ptr<Message> DefaultMessageQueue::tryPop() {
    std::unique_lock<std::mutex> lock(mutex_);
    
    // If queue is empty, return nullptr
    if (queue_.empty()) {
        return nullptr;
    }
    
    // Get message
    std::unique_ptr<Message> message = std::move(queue_.front());
    queue_.pop();
    
    // Notify waiting producers
    lock.unlock();
    if (max_size_ > 0) {
        not_full_.notify_one();
    }
    
    return message;
}

std::unique_ptr<Message> DefaultMessageQueue::waitAndPop(const std::chrono::milliseconds& timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    // Wait for queue to be non-empty or shutdown
    bool success = not_empty_.wait_for(lock, timeout, [this] { 
        return !queue_.empty() || shutdown_; 
    });
    
    // If timeout or queue is shutdown and empty, return nullptr
    if (!success || queue_.empty()) {
        return nullptr;
    }
    
    // Get message
    std::unique_ptr<Message> message = std::move(queue_.front());
    queue_.pop();
    
    // Notify waiting producers
    lock.unlock();
    if (max_size_ > 0) {
        not_full_.notify_one();
    }
    
    return message;
}

size_t DefaultMessageQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

bool DefaultMessageQueue::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
}

void DefaultMessageQueue::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::queue<std::unique_ptr<Message>> empty;
    std::swap(queue_, empty);
    if (max_size_ > 0) {
        not_full_.notify_all();
    }
}

void DefaultMessageQueue::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    shutdown_ = true;
    not_empty_.notify_all();
    not_full_.notify_all();
}

bool DefaultMessageQueue::isShutdown() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return shutdown_;
}

// PriorityMessageQueue implementation

PriorityMessageQueue::~PriorityMessageQueue() {
    shutdown();
}

bool PriorityMessageQueue::PriorityCompare::operator()(
    const std::pair<int, std::unique_ptr<Message>>& a,
    const std::pair<int, std::unique_ptr<Message>>& b
) const {
    // Higher priority value means higher priority (reverse of default behavior)
    return a.first < b.first;
}

void PriorityMessageQueue::push(std::unique_ptr<Message> message) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    // If queue is shutdown, discard message
    if (shutdown_) {
        NEXT_GEN_LOG_WARNING("Attempt to push message to shutdown priority queue");
        return;
    }
    
    // If queue is full, wait
    if (max_size_ > 0 && queue_.size() >= max_size_) {
        NEXT_GEN_LOG_WARNING("Priority message queue is full, waiting for space");
        not_full_.wait(lock, [this] { 
            return queue_.size() < max_size_ || shutdown_; 
        });
        
        // Check again if queue is shutdown
        if (shutdown_) {
            NEXT_GEN_LOG_WARNING("Priority queue shutdown while waiting to push message");
            return;
        }
    }
    
    // Calculate priority
    int priority = calculatePriority(*message);
    
    // Push message with priority
    queue_.push(std::make_pair(priority, std::move(message)));
    
    // Notify waiting consumers
    lock.unlock();
    not_empty_.notify_one();
}

std::unique_ptr<Message> PriorityMessageQueue::pop() {
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
    if (max_size_ > 0) {
        not_full_.notify_one();
    }
    
    return message;
}

std::unique_ptr<Message> PriorityMessageQueue::tryPop() {
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

std::unique_ptr<Message> PriorityMessageQueue::waitAndPop(const std::chrono::milliseconds& timeout) {
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
    lock.unlock();
    not_full_.notify_one();
    
    return message;
}

size_t PriorityMessageQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

bool PriorityMessageQueue::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
}

void PriorityMessageQueue::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Create a new empty queue
    std::priority_queue<
        std::pair<int, std::unique_ptr<Message>>,
        std::vector<std::pair<int, std::unique_ptr<Message>>>,
        PriorityCompare
    > empty;
    
    // Swap with current queue
    std::swap(queue_, empty);
    
    // Notify waiting producers
    if (max_size_ > 0) {
        not_full_.notify_all();
    }
}

void PriorityMessageQueue::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    shutdown_ = true;
    not_empty_.notify_all();
    not_full_.notify_all();
}

bool PriorityMessageQueue::isShutdown() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return shutdown_;
}

int PriorityMessageQueue::calculatePriority(const Message& message) const {
    // Default implementation: calculate priority based on message category
    // Higher category means higher priority
    return static_cast<int>(message.getCategory());
}

// LockFreeMessageQueue implementation

LockFreeMessageQueue::~LockFreeMessageQueue() {
    shutdown();
    clear();
    delete[] buffer_;
}

void LockFreeMessageQueue::push(std::unique_ptr<Message> message) {
    if (isShutdown()) {
        NEXT_GEN_LOG_WARNING("Attempt to push message to shutdown lock-free queue");
        return;
    }
    
    Message* raw_msg = message.release();
    
    while (true) {
        size_t head = head_.load(std::memory_order_relaxed);
        size_t tail = tail_.load(std::memory_order_acquire);
        
        // Check if queue is full
        if ((tail + 1) % (capacity_ + 1) == head) {
            NEXT_GEN_LOG_WARNING("Lock-free message queue is full, retrying");
            // Re-acquire ownership of the message to avoid memory leak
            message.reset(raw_msg);
            // Yield to allow other threads to make progress
            std::this_thread::yield();
            
            // If queue was shutdown while waiting, discard message
            if (isShutdown()) {
                NEXT_GEN_LOG_WARNING("Lock-free queue shutdown while waiting to push message");
                return;
            }
            
            // Release ownership again for next attempt
            raw_msg = message.release();
            continue;
        }
        
        // Try to store the message
        if (buffer_[tail].load(std::memory_order_relaxed) != nullptr) {
            // Another thread modified the tail, retry
            continue;
        }
        
        buffer_[tail].store(raw_msg, std::memory_order_relaxed);
        
        // Update tail
        tail_.store((tail + 1) % (capacity_ + 1), std::memory_order_release);
        return;
    }
}

std::unique_ptr<Message> LockFreeMessageQueue::pop() {
    while (true) {
        // If queue is empty and shutdown, return nullptr
        if (empty() && isShutdown()) {
            return nullptr;
        }
        
        // Try to pop non-blocking
        std::unique_ptr<Message> result = tryPop();
        if (result) {
            return result;
        }
        
        // If queue is shutdown, return nullptr
        if (isShutdown()) {
            return nullptr;
        }
        
        // Yield to allow other threads to make progress
        std::this_thread::yield();
    }
}

std::unique_ptr<Message> LockFreeMessageQueue::tryPop() {
    while (true) {
        size_t head = head_.load(std::memory_order_relaxed);
        size_t tail = tail_.load(std::memory_order_acquire);
        
        // Check if queue is empty
        if (head == tail) {
            return nullptr;
        }
        
        // Get the message
        Message* msg = buffer_[head].load(std::memory_order_relaxed);
        if (msg == nullptr) {
            // Another thread is in the middle of a push, retry
            continue;
        }
        
        // Try to update head
        if (head_.compare_exchange_strong(head, (head + 1) % (capacity_ + 1), 
                                         std::memory_order_release, 
                                         std::memory_order_relaxed)) {
            // Successfully popped the message
            buffer_[head].store(nullptr, std::memory_order_relaxed);
            return std::unique_ptr<Message>(msg);
        }
        
        // Another thread modified the head, retry
    }
}

std::unique_ptr<Message> LockFreeMessageQueue::waitAndPop(const std::chrono::milliseconds& timeout) {
    auto start = std::chrono::steady_clock::now();
    
    while (true) {
        // If queue is empty and shutdown, return nullptr
        if (empty() && isShutdown()) {
            return nullptr;
        }
        
        // Try to pop non-blocking
        std::unique_ptr<Message> result = tryPop();
        if (result) {
            return result;
        }
        
        // If queue is shutdown, return nullptr
        if (isShutdown()) {
            return nullptr;
        }
        
        // Check if timeout has elapsed
        auto now = std::chrono::steady_clock::now();
        if (now - start >= timeout) {
            return nullptr;
        }
        
        // Yield to allow other threads to make progress
        std::this_thread::yield();
    }
}

size_t LockFreeMessageQueue::size() const {
    size_t head = head_.load(std::memory_order_relaxed);
    size_t tail = tail_.load(std::memory_order_relaxed);
    
    if (tail >= head) {
        return tail - head;
    } else {
        return (capacity_ + 1) - (head - tail);
    }
}

bool LockFreeMessageQueue::empty() const {
    return head_.load(std::memory_order_relaxed) == 
           tail_.load(std::memory_order_relaxed);
}

void LockFreeMessageQueue::clear() {
    while (true) {
        std::unique_ptr<Message> msg = tryPop();
        if (!msg) {
            break;
        }
        // Message will be automatically deleted when unique_ptr goes out of scope
    }
}

void LockFreeMessageQueue::shutdown() {
    shutdown_.store(true, std::memory_order_release);
}

bool LockFreeMessageQueue::isShutdown() const {
    return shutdown_.load(std::memory_order_acquire);
}

// MPMCMessageQueue implementation

MPMCMessageQueue::~MPMCMessageQueue() {
    shutdown();
    clear();
    delete[] buffer_;
}

void MPMCMessageQueue::push(std::unique_ptr<Message> message) {
    if (isShutdown()) {
        NEXT_GEN_LOG_WARNING("Attempt to push message to shutdown MPMC queue");
        return;
    }
    
    Cell* cell;
    size_t pos;
    size_t seq;
    
    while (true) {
        pos = enqueue_pos_.load(std::memory_order_relaxed);
        cell = &buffer_[pos % capacity_];
        seq = cell->sequence.load(std::memory_order_acquire);
        
        intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
        
        // Cell is ready for enqueue
        if (diff == 0) {
            // Try to reserve this cell
            if (enqueue_pos_.compare_exchange_weak(pos, pos + 1, 
                                                 std::memory_order_relaxed)) {
                break;
            }
        }
        // Queue is full
        else if (diff < 0) {
            NEXT_GEN_LOG_WARNING("MPMC message queue is full, retrying");
            
            // If queue was shutdown while waiting, discard message
            if (isShutdown()) {
                NEXT_GEN_LOG_WARNING("MPMC queue shutdown while waiting to push message");
                return;
            }
            
            // Yield to allow other threads to make progress
            std::this_thread::yield();
        }
        // Another thread is in the middle of enqueue, retry
        else {
            std::this_thread::yield();
        }
    }
    
    // Store the message
    cell->data = message.release();
    
    // Mark the cell as ready for dequeue
    cell->sequence.store(pos + 1, std::memory_order_release);
}

std::unique_ptr<Message> MPMCMessageQueue::pop() {
    while (true) {
        // If queue is empty and shutdown, return nullptr
        if (empty() && isShutdown()) {
            return nullptr;
        }
        
        // Try to pop non-blocking
        std::unique_ptr<Message> result = tryPop();
        if (result) {
            return result;
        }
        
        // If queue is shutdown, return nullptr
        if (isShutdown()) {
            return nullptr;
        }
        
        // Yield to allow other threads to make progress
        std::this_thread::yield();
    }
}

std::unique_ptr<Message> MPMCMessageQueue::tryPop() {
    Cell* cell;
    size_t pos;
    size_t seq;
    
    while (true) {
        pos = dequeue_pos_.load(std::memory_order_relaxed);
        cell = &buffer_[pos % capacity_];
        seq = cell->sequence.load(std::memory_order_acquire);
        
        intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);
        
        // Cell is ready for dequeue
        if (diff == 0) {
            // Try to reserve this cell
            if (dequeue_pos_.compare_exchange_weak(pos, pos + 1, 
                                                 std::memory_order_relaxed)) {
                break;
            }
        }
        // Queue is empty
        else if (diff < 0) {
            return nullptr;
        }
        // Another thread is in the middle of dequeue, retry
        else {
            std::this_thread::yield();
        }
    }
    
    // Get the message
    Message* msg = cell->data;
    
    // Mark the cell as ready for enqueue
    cell->sequence.store(pos + capacity_, std::memory_order_release);
    
    return std::unique_ptr<Message>(msg);
}

std::unique_ptr<Message> MPMCMessageQueue::waitAndPop(const std::chrono::milliseconds& timeout) {
    auto start = std::chrono::steady_clock::now();
    
    while (true) {
        // If queue is empty and shutdown, return nullptr
        if (empty() && isShutdown()) {
            return nullptr;
        }
        
        // Try to pop non-blocking
        std::unique_ptr<Message> result = tryPop();
        if (result) {
            return result;
        }
        
        // If queue is shutdown, return nullptr
        if (isShutdown()) {
            return nullptr;
        }
        
        // Check if timeout has elapsed
        auto now = std::chrono::steady_clock::now();
        if (now - start >= timeout) {
            return nullptr;
        }
        
        // Yield to allow other threads to make progress
        std::this_thread::yield();
    }
}

size_t MPMCMessageQueue::size() const {
    size_t head = dequeue_pos_.load(std::memory_order_relaxed);
    size_t tail = enqueue_pos_.load(std::memory_order_relaxed);
    
    return tail - head;
}

bool MPMCMessageQueue::empty() const {
    return size() == 0;
}

void MPMCMessageQueue::clear() {
    while (true) {
        std::unique_ptr<Message> msg = tryPop();
        if (!msg) {
            break;
        }
        // Message will be automatically deleted when unique_ptr goes out of scope
    }
}

void MPMCMessageQueue::shutdown() {
    shutdown_.store(true, std::memory_order_release);
}

bool MPMCMessageQueue::isShutdown() const {
    return shutdown_.load(std::memory_order_acquire);
}

// Factory function implementation
std::unique_ptr<MessageQueue> createMessageQueue(const std::string& type, size_t capacity) {
    if (type == "default") {
        return std::make_unique<DefaultMessageQueue>(capacity);
    } else if (type == "priority") {
        return std::make_unique<PriorityMessageQueue>(capacity);
    } else if (type == "lockfree") {
        return std::make_unique<LockFreeMessageQueue>();
    } else if (type == "mpmc") {
        return std::make_unique<MPMCMessageQueue>(capacity);
    } else {
        NEXT_GEN_LOG_ERROR("Unknown message queue type: " + type);
        return std::make_unique<DefaultMessageQueue>(capacity);
    }
}

} // namespace next_gen
