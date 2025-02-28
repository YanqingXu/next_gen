#ifndef NEXT_GEN_SERVICE_H
#define NEXT_GEN_SERVICE_H

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <functional>
#include <vector>
#include <future>
#include <chrono>
#include "config.h"
#include "../message/message.h"
#include "../message/message_queue.h"
#include "../utils/logger.h"
#include "../utils/error.h"
#include "../module/module_interface.h"

namespace next_gen {

// Service interface
class NEXT_GEN_API Service {
public:
    virtual ~Service() = default;
    
    // Initialize service
    virtual Result<void> init() = 0;
    
    // Start service
    virtual Result<void> start() = 0;
    
    // Stop service
    virtual Result<void> stop() = 0;
    
    // Wait for service to end
    virtual Result<void> wait() = 0;
    
    // Post message to service
    virtual Result<void> postMessage(std::unique_ptr<Message> message) = 0;
    
    // Dispatch message
    virtual Result<void> dispatchMessage(const Message& message) = 0;
    
    // Register message handler
    virtual Result<void> registerMessageHandler(
        MessageCategoryType category,
        MessageIdType id,
        std::unique_ptr<MessageHandler> handler) = 0;
    
    // Register module
    virtual Result<void> registerModule(std::shared_ptr<ModuleInterface> module) = 0;
    
    // Get module by name
    virtual std::shared_ptr<ModuleInterface> getModule(const std::string& name) = 0;
    
    // Get service name
    virtual std::string getName() const = 0;
    
    // Check if service is running
    virtual bool isRunning() const = 0;
};

// Base service implementation
class NEXT_GEN_API BaseService : public Service {
public:
    BaseService(const std::string& name, std::shared_ptr<MessageQueue> queue = nullptr)
        : name_(name), 
          running_(false), 
          message_queue_(queue ? queue : std::make_shared<DefaultMessageQueue>()) {}
    
    virtual ~BaseService() {
        if (running_) {
            stop();
        }
    }
    
    // Initialize service
    Result<void> init() override {
        NEXT_GEN_LOG_INFO("Initializing service: " + name_);
        
        // Initialize message queue
        if (message_queue_->isShutdown()) {
            return Result<void>(ErrorCode::SERVICE_ERROR, "Message queue is shutdown");
        }
        
        // Call subclass initialization
        auto result = onInit();
        if (result.has_error()) {
            NEXT_GEN_LOG_ERROR("Failed to initialize service: " + name_ + ", error: " + 
                              result.error().what());
            return result;
        }
        
        NEXT_GEN_LOG_INFO("Service initialized: " + name_);
        return Result<void>();
    }
    
    // Start service
    Result<void> start() override {
        NEXT_GEN_LOG_INFO("Starting service: " + name_);
        
        // Check if service is already running
        if (running_) {
            return Result<void>(ErrorCode::SERVICE_ALREADY_STARTED, "Service already started");
        }
        
        // Set running flag
        running_ = true;
        
        // Start worker thread
        worker_thread_ = std::thread(&BaseService::run, this);
        
        // Call subclass start
        auto result = onStart();
        if (result.has_error()) {
            NEXT_GEN_LOG_ERROR("Failed to start service: " + name_ + ", error: " + 
                              result.error().what());
            running_ = false;
            if (worker_thread_.joinable()) {
                worker_thread_.join();
            }
            return result;
        }
        
        NEXT_GEN_LOG_INFO("Service started: " + name_);
        return Result<void>();
    }
    
    // Stop service
    Result<void> stop() override {
        NEXT_GEN_LOG_INFO("Stopping service: " + name_);
        
        // Check if service is running
        if (!running_) {
            return Result<void>(ErrorCode::SERVICE_NOT_STARTED, "Service not started");
        }
        
        // Set running flag
        running_ = false;
        
        // Shutdown message queue
        message_queue_->shutdown();
        
        // Call subclass stop
        auto result = onStop();
        if (result.has_error()) {
            NEXT_GEN_LOG_ERROR("Failed to stop service: " + name_ + ", error: " + 
                              result.error().what());
            return result;
        }
        
        // Wait for worker thread to finish
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
        
        NEXT_GEN_LOG_INFO("Service stopped: " + name_);
        return Result<void>();
    }
    
    // Wait for service to end
    Result<void> wait() override {
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
        return Result<void>();
    }
    
    // Post message to service
    Result<void> postMessage(std::unique_ptr<Message> message) override {
        // Check if service is running
        if (!running_) {
            return Result<void>(ErrorCode::SERVICE_NOT_STARTED, "Service not started");
        }
        
        // Set message timestamp
        message->setTimestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        
        // Post message to queue
        message_queue_->push(std::move(message));
        
        return Result<void>();
    }
    
    // Dispatch message
    Result<void> dispatchMessage(const Message& message) override {
        // Get message handler
        auto key = makeHandlerKey(message.getCategory(), message.getId());
        auto it = message_handlers_.find(key);
        if (it != message_handlers_.end()) {
            // Handle message
            it->second->handleMessage(message);
            return Result<void>();
        }
        
        // No handler found
        NEXT_GEN_LOG_WARNING("No handler for message: category=" + 
                           std::to_string(message.getCategory()) + 
                           ", id=" + std::to_string(message.getId()));
        return Result<void>(ErrorCode::MESSAGE_ERROR, "No handler for message");
    }
    
    // Register message handler
    Result<void> registerMessageHandler(
        MessageCategoryType category,
        MessageIdType id,
        std::unique_ptr<MessageHandler> handler) override {
        auto key = makeHandlerKey(category, id);
        message_handlers_[key] = std::move(handler);
        return Result<void>();
    }
    
    // Register message handler template method
    template<typename T, typename Handler>
    Result<void> registerMessageHandler(Handler&& handler) {
        static_assert(std::is_base_of<Message, T>::value, "T must be derived from Message");
        return registerMessageHandler(
            T::CATEGORY, 
            T::ID, 
            createMessageHandler<T>(std::forward<Handler>(handler))
        );
    }
    
    // Register module with explicit name
    Result<void> registerModuleWithName(const std::string& name, std::shared_ptr<ModuleInterface> module) {
        if (!module) {
            return Result<void>(ErrorCode::INVALID_ARGUMENT, "Module is null");
        }
        
        if (modules_.find(name) != modules_.end()) {
            return Result<void>(ErrorCode::MODULE_ALREADY_EXISTS, "Module already exists: " + name);
        }
        
        modules_[name] = module;
        return Result<void>();
    }
    
    // Register module
    Result<void> registerModule(std::shared_ptr<ModuleInterface> module) override {
        if (!module) {
            return Result<void>(ErrorCode::INVALID_ARGUMENT, "Module is null");
        }
        
        // 使用临时变量存储模块名称
        std::string name;
        
        // 使用try-catch块捕获任何可能的异常
        try {
            name = module->getName();
        } catch (const std::exception& e) {
            return Result<void>(ErrorCode::INVALID_ARGUMENT, 
                std::string("Failed to get module name: ") + e.what());
        }
        
        return registerModuleWithName(name, module);
    }
    
    // Get module by name
    std::shared_ptr<ModuleInterface> getModule(const std::string& name) override {
        auto it = modules_.find(name);
        if (it != modules_.end()) {
            return it->second;
        }
        return nullptr;
    }
    
    // Get service name
    std::string getName() const override {
        return name_;
    }
    
    // Check if service is running
    bool isRunning() const override {
        return running_;
    }
    
protected:
    // Subclass initialization method
    virtual Result<void> onInit() {
        return Result<void>();
    }
    
    // Subclass start method
    virtual Result<void> onStart() {
        return Result<void>();
    }
    
    // Subclass stop method
    virtual Result<void> onStop() {
        return Result<void>();
    }
    
    // Message handling method
    virtual Result<void> onMessage(const Message& message) {
        return dispatchMessage(message);
    }
    
    // Update method
    virtual Result<void> onUpdate(u64 elapsed_ms) {
        return Result<void>();
    }
    
private:
    // Main run loop
    void run() {
        NEXT_GEN_LOG_INFO("Service worker thread started: " + name_);
        
        auto last_update_time = std::chrono::steady_clock::now();
        
        while (running_) {
            // Process messages
            auto message = message_queue_->waitAndPop(std::chrono::milliseconds(100));
            if (message) {
                try {
                    onMessage(*message);
                } catch (const std::exception& e) {
                    NEXT_GEN_LOG_ERROR("Exception while processing message: " + std::string(e.what()));
                } catch (...) {
                    NEXT_GEN_LOG_ERROR("Unknown exception while processing message");
                }
            }
            
            // Calculate elapsed time
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update_time);
            
            // Call update method
            if (elapsed.count() > 0) {
                try {
                    onUpdate(elapsed.count());
                } catch (const std::exception& e) {
                    NEXT_GEN_LOG_ERROR("Exception in update: " + std::string(e.what()));
                } catch (...) {
                    NEXT_GEN_LOG_ERROR("Unknown exception in update");
                }
                last_update_time = now;
            }
        }
        
        NEXT_GEN_LOG_INFO("Service worker thread stopped: " + name_);
    }
    
    // Create handler key
    static u32 makeHandlerKey(MessageCategoryType category, MessageIdType id) {
        return (static_cast<u32>(category) << 16) | static_cast<u32>(id);
    }
    
    std::string name_;
    std::atomic<bool> running_;
    std::thread worker_thread_;
    std::shared_ptr<MessageQueue> message_queue_;
    std::unordered_map<u32, std::unique_ptr<MessageHandler>> message_handlers_;
    std::unordered_map<std::string, std::shared_ptr<ModuleInterface>> modules_;
};

} // namespace next_gen

#endif // NEXT_GEN_SERVICE_H
