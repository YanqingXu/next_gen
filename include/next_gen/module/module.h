#ifndef NEXT_GEN_MODULE_H
#define NEXT_GEN_MODULE_H

#include <string>
#include <memory>
#include "../core/service.h"
#include "../utils/error.h"

namespace next_gen {

// 模块接口
class NEXT_GEN_API Module : public std::enable_shared_from_this<Module> {
public:
    Module(std::weak_ptr<Service> service) : service_(service) {}
    
    virtual ~Module() = default;
    
    // 获取模块名称
    virtual std::string getName() const = 0;
    
    // 初始化模块
    virtual Result<void> init() {
        return Result<void>();
    }
    
    // 启动模块
    virtual Result<void> start() {
        return Result<void>();
    }
    
    // 停止模块
    virtual Result<void> stop() {
        return Result<void>();
    }
    
    // 更新模块
    virtual Result<void> update(u64 elapsed_ms) {
        return Result<void>();
    }
    
    // 注册消息处理器
    template<typename T, typename Handler>
    Result<void> registerMessageHandler(Handler&& handler) {
        static_assert(std::is_base_of<Message, T>::value, "T must be derived from Message");
        
        auto service = service_.lock();
        if (!service) {
            return Result<void>(ErrorCode::SERVICE_ERROR, "Service not available");
        }
        
        return service->registerMessageHandler(
            T::CATEGORY,
            T::ID,
            createMessageHandler<T>(std::forward<Handler>(handler))
        );
    }
    
    // 发送消息
    Result<void> postMessage(std::unique_ptr<Message> message) {
        auto service = service_.lock();
        if (!service) {
            return Result<void>(ErrorCode::SERVICE_ERROR, "Service not available");
        }
        
        return service->postMessage(std::move(message));
    }
    
    // 获取服务
    std::shared_ptr<Service> getService() const {
        return service_.lock();
    }
    
protected:
    std::weak_ptr<Service> service_;
};

// 基础模块实现
template<typename ModuleType>
class NEXT_GEN_API BaseModule : public Module {
public:
    BaseModule(std::weak_ptr<Service> service) : Module(service) {}
    
    // 获取模块名称
    std::string getName() const override {
        return ModuleType::MODULE_NAME;
    }
    
    // 获取模块实例
    static std::shared_ptr<ModuleType> getInstance(std::shared_ptr<Service> service) {
        auto module = std::make_shared<ModuleType>(service);
        auto result = service->registerModule(module);
        if (result.has_error()) {
            NEXT_GEN_LOG_ERROR("Failed to register module: " + ModuleType::MODULE_NAME + 
                              ", error: " + result.error().message());
            return nullptr;
        }
        return module;
    }
};

// 模块工厂
class NEXT_GEN_API ModuleFactory {
public:
    // 创建并注册模块
    template<typename ModuleType, typename... Args>
    static std::shared_ptr<ModuleType> createModule(std::shared_ptr<Service> service, Args&&... args) {
        static_assert(std::is_base_of<Module, ModuleType>::value, "ModuleType must be derived from Module");
        
        auto module = std::make_shared<ModuleType>(service, std::forward<Args>(args)...);
        auto result = service->registerModule(module);
        if (result.has_error()) {
            NEXT_GEN_LOG_ERROR("Failed to register module: " + module->getName() + 
                              ", error: " + result.error().message());
            return nullptr;
        }
        
        auto init_result = module->init();
        if (init_result.has_error()) {
            NEXT_GEN_LOG_ERROR("Failed to initialize module: " + module->getName() + 
                              ", error: " + init_result.error().message());
            return nullptr;
        }
        
        return module;
    }
};

} // namespace next_gen

// 便捷宏，用于定义模块
#define NEXT_GEN_DEFINE_MODULE(ModuleName) \
    static constexpr const char* MODULE_NAME = #ModuleName;

#endif // NEXT_GEN_MODULE_H
