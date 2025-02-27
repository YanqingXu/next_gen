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

namespace next_gen {

// 前向声明
class Module;

// 服务接口
class NEXT_GEN_API Service {
public:
    virtual ~Service() = default;
    
    // 初始化服务
    virtual Result<void> init() = 0;
    
    // 启动服务
    virtual Result<void> start() = 0;
    
    // 停止服务
    virtual Result<void> stop() = 0;
    
    // 等待服务结束
    virtual Result<void> wait() = 0;
    
    // 发送消息到服务
    virtual Result<void> postMessage(std::unique_ptr<Message> message) = 0;
    
    // 分发消息
    virtual Result<void> dispatchMessage(const Message& message) = 0;
    
    // 注册消息处理器
    virtual Result<void> registerMessageHandler(
        MessageCategoryType category,
        MessageIdType id,
        std::unique_ptr<MessageHandler> handler) = 0;
    
    // 注册模块
    virtual Result<void> registerModule(std::shared_ptr<Module> module) = 0;
    
    // 获取模块
    virtual std::shared_ptr<Module> getModule(const std::string& name) = 0;
    
    // 获取服务名称
    virtual std::string getName() const = 0;
    
    // 获取服务状态
    virtual bool isRunning() const = 0;
};

// 基础服务实现
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
    
    // 初始化服务
    Result<void> init() override {
        NEXT_GEN_LOG_INFO("Initializing service: " + name_);
        
        // 初始化消息队列
        if (message_queue_->isShutdown()) {
            return Result<void>(ErrorCode::SERVICE_ERROR, "Message queue is shutdown");
        }
        
        // 调用子类初始化
        auto result = onInit();
        if (result.has_error()) {
            NEXT_GEN_LOG_ERROR("Failed to initialize service: " + name_ + ", error: " + 
                              result.error().message());
            return result;
        }
        
        NEXT_GEN_LOG_INFO("Service initialized: " + name_);
        return Result<void>();
    }
    
    // 启动服务
    Result<void> start() override {
        NEXT_GEN_LOG_INFO("Starting service: " + name_);
        
        // 检查服务是否已经运行
        if (running_) {
            return Result<void>(ErrorCode::SERVICE_ALREADY_STARTED, "Service already started");
        }
        
        // 设置运行标志
        running_ = true;
        
        // 启动工作线程
        worker_thread_ = std::thread(&BaseService::run, this);
        
        // 调用子类启动
        auto result = onStart();
        if (result.has_error()) {
            NEXT_GEN_LOG_ERROR("Failed to start service: " + name_ + ", error: " + 
                              result.error().message());
            running_ = false;
            if (worker_thread_.joinable()) {
                worker_thread_.join();
            }
            return result;
        }
        
        NEXT_GEN_LOG_INFO("Service started: " + name_);
        return Result<void>();
    }
    
    // 停止服务
    Result<void> stop() override {
        NEXT_GEN_LOG_INFO("Stopping service: " + name_);
        
        // 检查服务是否正在运行
        if (!running_) {
            return Result<void>(ErrorCode::SERVICE_NOT_STARTED, "Service not started");
        }
        
        // 设置运行标志
        running_ = false;
        
        // 关闭消息队列
        message_queue_->shutdown();
        
        // 调用子类停止
        auto result = onStop();
        if (result.has_error()) {
            NEXT_GEN_LOG_ERROR("Failed to stop service: " + name_ + ", error: " + 
                              result.error().message());
            return result;
        }
        
        // 等待工作线程结束
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
        
        NEXT_GEN_LOG_INFO("Service stopped: " + name_);
        return Result<void>();
    }
    
    // 等待服务结束
    Result<void> wait() override {
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
        return Result<void>();
    }
    
    // 发送消息到服务
    Result<void> postMessage(std::unique_ptr<Message> message) override {
        // 检查服务是否正在运行
        if (!running_) {
            return Result<void>(ErrorCode::SERVICE_NOT_STARTED, "Service not started");
        }
        
        // 设置消息时间戳
        message->setTimestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        
        // 发送消息到队列
        message_queue_->push(std::move(message));
        
        return Result<void>();
    }
    
    // 分发消息
    Result<void> dispatchMessage(const Message& message) override {
        // 获取消息处理器
        auto key = makeHandlerKey(message.getCategory(), message.getId());
        auto it = message_handlers_.find(key);
        if (it != message_handlers_.end()) {
            // 处理消息
            it->second->handleMessage(message);
            return Result<void>();
        }
        
        // 未找到处理器
        NEXT_GEN_LOG_WARNING("No handler for message: category=" + 
                           std::to_string(message.getCategory()) + 
                           ", id=" + std::to_string(message.getId()));
        return Result<void>(ErrorCode::MESSAGE_ERROR, "No handler for message");
    }
    
    // 注册消息处理器
    Result<void> registerMessageHandler(
        MessageCategoryType category,
        MessageIdType id,
        std::unique_ptr<MessageHandler> handler) override {
        auto key = makeHandlerKey(category, id);
        message_handlers_[key] = std::move(handler);
        return Result<void>();
    }
    
    // 便捷的消息处理器注册模板方法
    template<typename T, typename Handler>
    Result<void> registerMessageHandler(Handler&& handler) {
        static_assert(std::is_base_of<Message, T>::value, "T must be derived from Message");
        return registerMessageHandler(
            T::CATEGORY, 
            T::ID, 
            createMessageHandler<T>(std::forward<Handler>(handler))
        );
    }
    
    // 注册模块
    Result<void> registerModule(std::shared_ptr<Module> module) override {
        if (!module) {
            return Result<void>(ErrorCode::INVALID_ARGUMENT, "Module is null");
        }
        
        std::string name = module->getName();
        if (modules_.find(name) != modules_.end()) {
            return Result<void>(ErrorCode::MODULE_ALREADY_EXISTS, "Module already exists: " + name);
        }
        
        modules_[name] = module;
        return Result<void>();
    }
    
    // 获取模块
    std::shared_ptr<Module> getModule(const std::string& name) override {
        auto it = modules_.find(name);
        if (it != modules_.end()) {
            return it->second;
        }
        return nullptr;
    }
    
    // 获取服务名称
    std::string getName() const override {
        return name_;
    }
    
    // 获取服务状态
    bool isRunning() const override {
        return running_;
    }
    
protected:
    // 子类初始化方法
    virtual Result<void> onInit() {
        return Result<void>();
    }
    
    // 子类启动方法
    virtual Result<void> onStart() {
        return Result<void>();
    }
    
    // 子类停止方法
    virtual Result<void> onStop() {
        return Result<void>();
    }
    
    // 消息处理方法
    virtual Result<void> onMessage(const Message& message) {
        return dispatchMessage(message);
    }
    
    // 更新方法
    virtual Result<void> onUpdate(u64 elapsed_ms) {
        return Result<void>();
    }
    
private:
    // 主运行循环
    void run() {
        NEXT_GEN_LOG_INFO("Service worker thread started: " + name_);
        
        auto last_update_time = std::chrono::steady_clock::now();
        
        while (running_) {
            // 处理消息
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
            
            // 计算经过的时间
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update_time);
            
            // 调用更新方法
            if (elapsed.count() > 0) {
                try {
                    onUpdate(elapsed.count());
                } catch (const std::exception& e) {
                    NEXT_GEN_LOG_ERROR("Exception in onUpdate: " + std::string(e.what()));
                } catch (...) {
                    NEXT_GEN_LOG_ERROR("Unknown exception in onUpdate");
                }
                last_update_time = now;
            }
        }
        
        NEXT_GEN_LOG_INFO("Service worker thread stopped: " + name_);
    }
    
    // 创建处理器键
    static u32 makeHandlerKey(MessageCategoryType category, MessageIdType id) {
        return (static_cast<u32>(category) << 16) | static_cast<u32>(id);
    }
    
    std::string name_;
    std::atomic<bool> running_;
    std::thread worker_thread_;
    std::shared_ptr<MessageQueue> message_queue_;
    std::unordered_map<u32, std::unique_ptr<MessageHandler>> message_handlers_;
    std::unordered_map<std::string, std::shared_ptr<Module>> modules_;
};

} // namespace next_gen

#endif // NEXT_GEN_SERVICE_H
