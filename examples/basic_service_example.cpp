#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include "../include/core/service.h"
#include "../include/module/module_impl.h"
#include "../include/module/module.h"
#include "../include/message/message.h"
#include "../include/message/message_queue.h"
#include "../include/utils/logger.h"
#include "../include/utils/timer.h"

using namespace next_gen;

// Custom message types
enum class CustomMessageCategory : MessageCategoryType {
    SYSTEM = 1,
    USER = 2
};

// System message IDs
enum class SystemMessageId : MessageIdType {
    PING = 1,
    PONG = 2,
    SHUTDOWN = 3
};

// User message IDs
enum class UserMessageId : MessageIdType {
    LOGIN = 1,
    LOGOUT = 2,
    CHAT = 3
};

// Ping message
class PingMessage : public Message {
public:
    static constexpr MessageCategoryType CATEGORY = static_cast<MessageCategoryType>(CustomMessageCategory::SYSTEM);
    static constexpr MessageIdType ID = static_cast<MessageIdType>(SystemMessageId::PING);
    
    PingMessage() : Message(CATEGORY, ID) {}
    
    // Serialize
    Result<std::vector<u8>> serialize() const override {
        std::vector<u8> data;
        data.push_back(static_cast<u8>(timestamp_ >> 56));
        data.push_back(static_cast<u8>(timestamp_ >> 48));
        data.push_back(static_cast<u8>(timestamp_ >> 40));
        data.push_back(static_cast<u8>(timestamp_ >> 32));
        data.push_back(static_cast<u8>(timestamp_ >> 24));
        data.push_back(static_cast<u8>(timestamp_ >> 16));
        data.push_back(static_cast<u8>(timestamp_ >> 8));
        data.push_back(static_cast<u8>(timestamp_));
        return Result<std::vector<u8>>(data);
    }
    
    // Deserialize
    Result<void> deserialize(const std::vector<u8>& data) override {
        if (data.size() < 8) {
            return Result<void>(ErrorCode::MESSAGE_ERROR, "Invalid data size");
        }
        
        timestamp_ = static_cast<u64>(data[0]) << 56 |
                    static_cast<u64>(data[1]) << 48 |
                    static_cast<u64>(data[2]) << 40 |
                    static_cast<u64>(data[3]) << 32 |
                    static_cast<u64>(data[4]) << 24 |
                    static_cast<u64>(data[5]) << 16 |
                    static_cast<u64>(data[6]) << 8 |
                    static_cast<u64>(data[7]);
        return Result<void>();
    }
};

// Pong message
class PongMessage : public Message {
public:
    static constexpr MessageCategoryType CATEGORY = static_cast<MessageCategoryType>(CustomMessageCategory::SYSTEM);
    static constexpr MessageIdType ID = static_cast<MessageIdType>(SystemMessageId::PONG);
    
    PongMessage() : Message(CATEGORY, ID) {}
    
    // Serialize
    Result<std::vector<u8>> serialize() const override {
        std::vector<u8> data;
        data.push_back(static_cast<u8>(timestamp_ >> 56));
        data.push_back(static_cast<u8>(timestamp_ >> 48));
        data.push_back(static_cast<u8>(timestamp_ >> 40));
        data.push_back(static_cast<u8>(timestamp_ >> 32));
        data.push_back(static_cast<u8>(timestamp_ >> 24));
        data.push_back(static_cast<u8>(timestamp_ >> 16));
        data.push_back(static_cast<u8>(timestamp_ >> 8));
        data.push_back(static_cast<u8>(timestamp_));
        return Result<std::vector<u8>>(data);
    }
    
    // Deserialize
    Result<void> deserialize(const std::vector<u8>& data) override {
        if (data.size() < 8) {
            return Result<void>(ErrorCode::MESSAGE_ERROR, "Invalid data size");
        }
        
        timestamp_ = static_cast<u64>(data[0]) << 56 |
                    static_cast<u64>(data[1]) << 48 |
                    static_cast<u64>(data[2]) << 40 |
                    static_cast<u64>(data[3]) << 32 |
                    static_cast<u64>(data[4]) << 24 |
                    static_cast<u64>(data[5]) << 16 |
                    static_cast<u64>(data[6]) << 8 |
                    static_cast<u64>(data[7]);
        return Result<void>();
    }
};

// Shutdown message
class ShutdownMessage : public Message {
public:
    static constexpr MessageCategoryType CATEGORY = static_cast<MessageCategoryType>(CustomMessageCategory::SYSTEM);
    static constexpr MessageIdType ID = static_cast<MessageIdType>(SystemMessageId::SHUTDOWN);
    
    ShutdownMessage() : Message(CATEGORY, ID) {}
    
    // Serialize
    Result<std::vector<u8>> serialize() const override {
        return Result<std::vector<u8>>(std::vector<u8>());
    }
    
    // Deserialize
    Result<void> deserialize(const std::vector<u8>& data) override {
        return Result<void>();
    }
};

// Heartbeat module
class HeartbeatModule : public BaseModule<HeartbeatModule> {
public:
    NEXT_GEN_DEFINE_MODULE(Heartbeat)
    
    HeartbeatModule(std::weak_ptr<Service> service) : BaseModule(service), timer_id_(0) {}
    
    ~HeartbeatModule() {
        if (timer_id_ != 0) {
            Timer::cancel(timer_id_);
        }
    }
    
    // Initialize module
    Result<void> init() override {
        NEXT_GEN_LOG_INFO("Initializing heartbeat module");
        
        // Register message handlers
        auto result = registerMessageHandler<PingMessage>([this](const PingMessage& message) {
            handlePing(message);
        });
        
        if (result.has_error()) {
            NEXT_GEN_LOG_ERROR("Failed to register ping message handler: " + result.error().message());
            return result;
        }
        
        result = registerMessageHandler<PongMessage>([this](const PongMessage& message) {
            handlePong(message);
        });
        
        if (result.has_error()) {
            NEXT_GEN_LOG_ERROR("Failed to register pong message handler: " + result.error().message());
            return result;
        }
        
        result = registerMessageHandler<ShutdownMessage>([this](const ShutdownMessage& message) {
            handleShutdown(message);
        });
        
        if (result.has_error()) {
            NEXT_GEN_LOG_ERROR("Failed to register shutdown message handler: " + result.error().message());
            return result;
        }
        
        return Result<void>();
    }
    
    // Start module
    Result<void> start() override {
        NEXT_GEN_LOG_INFO("Starting heartbeat module");
        
        // Start heartbeat timer
        timer_id_ = Timer::repeat(1000, 5000, [this]() {
            sendPing();
        });
        
        return Result<void>();
    }
    
    // Stop module
    Result<void> stop() override {
        NEXT_GEN_LOG_INFO("Stopping heartbeat module");
        
        // Cancel heartbeat timer
        if (timer_id_ != 0) {
            Timer::cancel(timer_id_);
            timer_id_ = 0;
        }
        
        return Result<void>();
    }
    
private:
    // Send Ping message
    void sendPing() {
        NEXT_GEN_LOG_DEBUG("Sending ping message");
        
        auto message = std::make_unique<PingMessage>();
        auto service = getService();
        if (service) {
            service->postMessage(std::move(message));
        }
    }
    
    // Handle Ping message
    void handlePing(const PingMessage& message) {
        NEXT_GEN_LOG_DEBUG("Received ping message, timestamp: " + std::to_string(message.getTimestamp()));
        
        // Reply with Pong message
        auto pong = std::make_unique<PongMessage>();
        auto service = getService();
        if (service) {
            service->postMessage(std::move(pong));
        }
    }
    
    // Handle Pong message
    void handlePong(const PongMessage& message) {
        NEXT_GEN_LOG_DEBUG("Received pong message, timestamp: " + std::to_string(message.getTimestamp()));
    }
    
    // Handle Shutdown message
    void handleShutdown(const ShutdownMessage& message) {
        NEXT_GEN_LOG_INFO("Received shutdown message");
        
        auto service = getService();
        if (service) {
            service->stop();
        }
    }
    
    TimerId timer_id_;
};

// Example service
class ExampleService : public BaseService {
public:
    ExampleService() : BaseService("ExampleService") {}
    
    // Initialize service
    Result<void> onInit() override {
        NEXT_GEN_LOG_INFO("Initializing example service");
        
        // Register heartbeat module
        auto heartbeat = ModuleFactory::createModule<HeartbeatModule>(shared_from_this());
        if (!heartbeat) {
            return Result<void>(ErrorCode::MODULE_ERROR, "Failed to create heartbeat module");
        }
        
        return Result<void>();
    }
    
    // Start service
    Result<void> onStart() override {
        NEXT_GEN_LOG_INFO("Starting example service");
        
        // Start shutdown timer
        Timer::once(30000, [this]() {
            NEXT_GEN_LOG_INFO("Sending shutdown message");
            auto message = std::make_unique<ShutdownMessage>();
            postMessage(std::move(message));
        });
        
        return Result<void>();
    }
    
    // Stop service
    Result<void> onStop() override {
        NEXT_GEN_LOG_INFO("Stopping example service");
        return Result<void>();
    }
};

int main() {
    // Initialize logging system
    LogManager::instance().addSink(std::make_shared<ConsoleSink>(LogLevel::DEBUG));
    LogManager::instance().addSink(std::make_shared<FileSink>("example_service.log", LogLevel::INFO));
    
    NEXT_GEN_LOG_INFO("Starting example service application");
    
    // Create service
    auto service = std::make_shared<ExampleService>();
    
    // Initialize service
    auto result = service->init();
    if (result.has_error()) {
        NEXT_GEN_LOG_ERROR("Failed to initialize service: " + result.error().message());
        return 1;
    }
    
    // Start service
    result = service->start();
    if (result.has_error()) {
        NEXT_GEN_LOG_ERROR("Failed to start service: " + result.error().message());
        return 1;
    }
    
    // Wait for service to finish
    service->wait();
    
    NEXT_GEN_LOG_INFO("Example service application stopped");
    
    return 0;
}
