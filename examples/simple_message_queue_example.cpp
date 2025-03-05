#include "../include/message/message_queue.h"
#include "../include/utils/logger.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <iomanip>
#include <functional>

using namespace next_gen;

// Example message class
class TestMessage : public Message {
public:
    TestMessage(u32 value) : Message(1, 1), value_(value) {}
    
    u32 getValue() const { return value_; }
    
    std::string getName() const override { return "TestMessage"; }
    
    std::string toString() const override {
        return "TestMessage: value=" + std::to_string(value_);
    }
    
private:
    u32 value_;
};

// Performance test function for DefaultMessageQueue
void runDefaultQueueTest(size_t numProducers, size_t numConsumers, 
                        size_t messagesPerProducer, size_t queueCapacity = 1024) {
    std::cout << "\nRunning performance test for DefaultMessageQueue:" << std::endl;
    std::cout << "- " << numProducers << " producers" << std::endl;
    std::cout << "- " << numConsumers << " consumers" << std::endl;
    std::cout << "- " << messagesPerProducer << " messages per producer" << std::endl;
    std::cout << "- " << queueCapacity << " queue capacity" << std::endl;
    
    // Create message queue
    auto queue = std::make_unique<DefaultMessageQueue>(queueCapacity);
    
    // Counters
    std::atomic<size_t> messagesProduced(0);
    std::atomic<size_t> messagesConsumed(0);
    std::atomic<bool> done(false);
    
    // Producer function
    auto producerFunc = [&](int producerId) {
        for (size_t i = 0; i < messagesPerProducer; ++i) {
            u32 value = (producerId * messagesPerProducer) + i;
            auto message = std::make_unique<TestMessage>(value);
            queue->push(std::move(message));
            messagesProduced.fetch_add(1, std::memory_order_relaxed);
        }
    };
    
    // Consumer function
    auto consumerFunc = [&]() {
        while (!done.load(std::memory_order_acquire) || !queue->empty()) {
            auto message = queue->tryPop();
            if (message) {
                messagesConsumed.fetch_add(1, std::memory_order_relaxed);
            } else {
                // No message available, yield to other threads
                std::this_thread::yield();
            }
        }
    };
    
    // Start timing
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Create and start producer threads
    std::vector<std::thread> producers;
    for (size_t i = 0; i < numProducers; ++i) {
        producers.emplace_back(producerFunc, i);
    }
    
    // Create and start consumer threads
    std::vector<std::thread> consumers;
    for (size_t i = 0; i < numConsumers; ++i) {
        consumers.emplace_back(consumerFunc);
    }
    
    // Wait for all producers to finish
    for (auto& thread : producers) {
        thread.join();
    }
    
    // Signal consumers that production is done
    done.store(true, std::memory_order_release);
    
    // Wait for all consumers to finish
    for (auto& thread : consumers) {
        thread.join();
    }
    
    // End timing
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    
    // Calculate throughput
    double throughput = static_cast<double>(messagesProduced.load()) / (duration / 1000.0);
    
    // Print results
    std::cout << "Results:" << std::endl;
    std::cout << "- Total time: " << duration << " ms" << std::endl;
    std::cout << "- Messages produced: " << messagesProduced.load() << std::endl;
    std::cout << "- Messages consumed: " << messagesConsumed.load() << std::endl;
    std::cout << "- Throughput: " << std::fixed << std::setprecision(2) << throughput << " messages/second" << std::endl;
    
    // Verify all messages were consumed
    if (messagesProduced.load() != messagesConsumed.load()) {
        std::cout << "ERROR: Not all messages were consumed!" << std::endl;
    }
}

// Performance test function for PriorityMessageQueue
void runPriorityQueueTest(size_t numProducers, size_t numConsumers, 
                        size_t messagesPerProducer, size_t queueCapacity = 1024) {
    std::cout << "\nRunning performance test for PriorityMessageQueue:" << std::endl;
    std::cout << "- " << numProducers << " producers" << std::endl;
    std::cout << "- " << numConsumers << " consumers" << std::endl;
    std::cout << "- " << messagesPerProducer << " messages per producer" << std::endl;
    std::cout << "- " << queueCapacity << " queue capacity" << std::endl;
    
    // Create message queue
    auto queue = std::make_unique<PriorityMessageQueue>(queueCapacity);
    
    // Counters
    std::atomic<size_t> messagesProduced(0);
    std::atomic<size_t> messagesConsumed(0);
    std::atomic<bool> done(false);
    
    // Producer function
    auto producerFunc = [&](int producerId) {
        for (size_t i = 0; i < messagesPerProducer; ++i) {
            u32 value = (producerId * messagesPerProducer) + i;
            auto message = std::make_unique<TestMessage>(value);
            queue->push(std::move(message));
            messagesProduced.fetch_add(1, std::memory_order_relaxed);
        }
    };
    
    // Consumer function
    auto consumerFunc = [&]() {
        while (!done.load(std::memory_order_acquire) || !queue->empty()) {
            auto message = queue->tryPop();
            if (message) {
                messagesConsumed.fetch_add(1, std::memory_order_relaxed);
            } else {
                // No message available, yield to other threads
                std::this_thread::yield();
            }
        }
    };
    
    // Start timing
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Create and start producer threads
    std::vector<std::thread> producers;
    for (size_t i = 0; i < numProducers; ++i) {
        producers.emplace_back(producerFunc, i);
    }
    
    // Create and start consumer threads
    std::vector<std::thread> consumers;
    for (size_t i = 0; i < numConsumers; ++i) {
        consumers.emplace_back(consumerFunc);
    }
    
    // Wait for all producers to finish
    for (auto& thread : producers) {
        thread.join();
    }
    
    // Signal consumers that production is done
    done.store(true, std::memory_order_release);
    
    // Wait for all consumers to finish
    for (auto& thread : consumers) {
        thread.join();
    }
    
    // End timing
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    
    // Calculate throughput
    double throughput = static_cast<double>(messagesProduced.load()) / (duration / 1000.0);
    
    // Print results
    std::cout << "Results:" << std::endl;
    std::cout << "- Total time: " << duration << " ms" << std::endl;
    std::cout << "- Messages produced: " << messagesProduced.load() << std::endl;
    std::cout << "- Messages consumed: " << messagesConsumed.load() << std::endl;
    std::cout << "- Throughput: " << std::fixed << std::setprecision(2) << throughput << " messages/second" << std::endl;
    
    // Verify all messages were consumed
    if (messagesProduced.load() != messagesConsumed.load()) {
        std::cout << "ERROR: Not all messages were consumed!" << std::endl;
    }
}

// Example usage of different queue types
void demonstrateQueueTypes() {
    std::cout << "\nDemonstrating different queue types:" << std::endl;
    
    // Create different queue types
    auto defaultQueue = std::make_unique<DefaultMessageQueue>();
    auto priorityQueue = std::make_unique<PriorityMessageQueue>();
    
    // Push messages to each queue
    std::cout << "Pushing messages to queues..." << std::endl;
    
    for (int i = 0; i < 5; ++i) {
        defaultQueue->push(std::make_unique<TestMessage>(i));
        priorityQueue->push(std::make_unique<TestMessage>(i));
    }
    
    // Pop and print messages from each queue
    std::cout << "\nDefault Queue:" << std::endl;
    while (!defaultQueue->empty()) {
        auto message = defaultQueue->tryPop();
        if (message) {
            auto* testMessage = dynamic_cast<TestMessage*>(message.get());
            if (testMessage) {
                std::cout << "  " << testMessage->toString() << std::endl;
            }
        }
    }
    
    std::cout << "\nPriority Queue:" << std::endl;
    while (!priorityQueue->empty()) {
        auto message = priorityQueue->tryPop();
        if (message) {
            auto* testMessage = dynamic_cast<TestMessage*>(message.get());
            if (testMessage) {
                std::cout << "  " << testMessage->toString() << std::endl;
            }
        }
    }
}

int main() {
    // Initialize logger
    Logger::instance().init("message_queue_example.log", LogLevel::INFO);
    
    std::cout << "Simple Message Queue Example Started" << std::endl;
    
    // Demonstrate different queue types
    demonstrateQueueTypes();
    
    // Run performance tests
    
    // Test 1: Single producer, single consumer
    runDefaultQueueTest(1, 1, 100000);
    runPriorityQueueTest(1, 1, 100000);
    
    // Test 2: Multiple producers, single consumer
    runDefaultQueueTest(4, 1, 25000);
    runPriorityQueueTest(4, 1, 25000);
    
    // Test 3: Single producer, multiple consumers
    runDefaultQueueTest(1, 4, 100000);
    runPriorityQueueTest(1, 4, 100000);
    
    // Test 4: Multiple producers, multiple consumers
    runDefaultQueueTest(4, 4, 25000);
    runPriorityQueueTest(4, 4, 25000);
    
    std::cout << "\nSimple Message Queue Example Completed" << std::endl;
    
    return 0;
}
