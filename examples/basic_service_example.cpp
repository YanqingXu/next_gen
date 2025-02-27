#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <next_gen/core/service.h>
#include <next_gen/module/module.h>
#include <next_gen/message/message.h>
#include <next_gen/message/message_queue.h>
#include <next_gen/utils/logger.h>
#include <next_gen/utils/timer.h>

using namespace next_gen;

// 自定义消息类型
enum class CustomMessageCategory : MessageCategoryType {
    SYSTEM = 1,
    USER = 2
};

// 系统消息ID
enum class SystemMessageId : MessageIdType {
    PING = 1,
    PONG = 2,
    SHUTDOWN = 3
};

// 用户消息ID
enum class UserMessageId : MessageIdType {
    LOGIN = 1,
    LOGOUT = 2,
    CHAT = 3
};

// Ping消息
class PingMessage : public Message {
public:
    static constexpr MessageCategoryType CATEGORY = static_cast<MessageCategoryType>(CustomMessageCategory::SYSTEM);
    static constexpr MessageIdType ID = static_cast<MessageIdType>(SystemMessageId::PING);
    
    PingMessage() : Message(CATEGORY, ID) {}
    
    // 序列化
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
    
    // 反序列化
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

// Pong消息
class PongMessage : public Message {
public:
    static constexpr MessageCategoryType CATEGORY = static_cast<MessageCategoryType>(CustomMessageCategory::SYSTEM);
    static constexpr MessageIdType ID = static_cast<MessageIdType>(SystemMessageId::PONG);
    
    PongMessage() : Message(CATEGORY, ID) {}
    
    // 序列化
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
    
    // 反序列化
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

// 关闭消息
class ShutdownMessage : public Message {
public:
    static constexpr MessageCategoryType CATEGORY = static_cast<MessageCategoryType>(CustomMessageCategory::SYSTEM);
    static constexpr MessageIdType ID = static_cast<MessageIdType>(SystemMessageId::SHUTDOWN);
    
    ShutdownMessage() : Message(CATEGORY, ID) {}
    
    // 序列化
    Result<std::vector<u8>> serialize() const override {
        return Result<std::vector<u8>>(std::vector<u8>());
    }
    
    // 反序列化
    Result<void> deserialize(const std::vector<u8>& data) override {
        return Result<void>();
    }
};

// 心跳模块
class HeartbeatModule : public BaseModule<HeartbeatModule> {
public:
    NEXT_GEN_DEFINE_MODULE(Heartbeat)
    
    HeartbeatModule(std::weak_ptr<Service> service) : BaseModule(service), timer_id_(0) {}
    
    ~HeartbeatModule() {
        if (timer_id_ != 0) {
            Timer::cancel(timer_id_);
        }
    }
    
    // 初始化模块
    Result<void> init() override {
        NEXT_GEN_LOG_INFO("Initializing heartbeat module");
        
        // 注册消息处理器
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
    
    // 启动模块
    Result<void> start() override {
        NEXT_GEN_LOG_INFO("Starting heartbeat module");
        
        // 启动心跳定时器
        timer_id_ = Timer::repeat(1000, 5000, [this]() {
            sendPing();
        });
        
        return Result<void>();
    }
    
    // 停止模块
    Result<void> stop() override {
        NEXT_GEN_LOG_INFO("Stopping heartbeat module");
        
        // 取消心跳定时器
        if (timer_id_ != 0) {
            Timer::cancel(timer_id_);
            timer_id_ = 0;
        }
        
        return Result<void>();
    }
    
private:
    // 发送Ping消息
    void sendPing() {
        NEXT_GEN_LOG_DEBUG("Sending ping message");
        
        auto message = std::make_unique<PingMessage>();
        auto service = getService();
        if (service) {
            service->postMessage(std::move(message));
        }
    }
    
    // 处理Ping消息
    void handlePing(const PingMessage& message) {
        NEXT_GEN_LOG_DEBUG("Received ping message, timestamp: " + std::to_string(message.getTimestamp()));
        
        // 回复Pong消息
        auto pong = std::make_unique<PongMessage>();
        auto service = getService();
        if (service) {
            service->postMessage(std::move(pong));
        }
    }
    
    // 处理Pong消息
    void handlePong(const PongMessage& message) {
        NEXT_GEN_LOG_DEBUG("Received pong message, timestamp: " + std::to_string(message.getTimestamp()));
    }
    
    // 处理关闭消息
    void handleShutdown(const ShutdownMessage& message) {
        NEXT_GEN_LOG_INFO("Received shutdown message");
        
        auto service = getService();
        if (service) {
            service->stop();
        }
    }
    
    TimerId timer_id_;
};

// 示例服务
class ExampleService : public BaseService {
public:
    ExampleService() : BaseService("ExampleService") {}
    
    // 初始化服务
    Result<void> onInit() override {
        NEXT_GEN_LOG_INFO("Initializing example service");
        
        // 注册心跳模块
        auto heartbeat = ModuleFactory::createModule<HeartbeatModule>(shared_from_this());
        if (!heartbeat) {
            return Result<void>(ErrorCode::MODULE_ERROR, "Failed to create heartbeat module");
        }
        
        return Result<void>();
    }
    
    // 启动服务
    Result<void> onStart() override {
        NEXT_GEN_LOG_INFO("Starting example service");
        
        // 启动关闭定时器
        Timer::once(30000, [this]() {
            NEXT_GEN_LOG_INFO("Sending shutdown message");
            auto message = std::make_unique<ShutdownMessage>();
            postMessage(std::move(message));
        });
        
        return Result<void>();
    }
    
    // 停止服务
    Result<void> onStop() override {
        NEXT_GEN_LOG_INFO("Stopping example service");
        return Result<void>();
    }
};

int main() {
    // 初始化日志系统
    LogManager::instance().addSink(std::make_shared<ConsoleSink>(LogLevel::DEBUG));
    LogManager::instance().addSink(std::make_shared<FileSink>("example_service.log", LogLevel::INFO));
    
    NEXT_GEN_LOG_INFO("Starting example service application");
    
    // 创建服务
    auto service = std::make_shared<ExampleService>();
    
    // 初始化服务
    auto result = service->init();
    if (result.has_error()) {
        NEXT_GEN_LOG_ERROR("Failed to initialize service: " + result.error().message());
        return 1;
    }
    
    // 启动服务
    result = service->start();
    if (result.has_error()) {
        NEXT_GEN_LOG_ERROR("Failed to start service: " + result.error().message());
        return 1;
    }
    
    // 等待服务结束
    service->wait();
    
    NEXT_GEN_LOG_INFO("Example service application stopped");
    
    return 0;
}
