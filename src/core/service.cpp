#include "../../include/core/service.h"
#include <chrono>
#include <algorithm>

namespace next_gen {

// Base service implementation methods

Result<void> BaseService::registerMessageHandler(
    MessageCategoryType category,
    MessageIdType id,
    std::unique_ptr<MessageHandler> handler) {
    
    if (!handler) {
        return Result<void>(ErrorCode::INVALID_ARGUMENT, "Handler cannot be null");
    }
    
    auto key = makeHandlerKey(category, id);
    
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    auto it = message_handlers_.find(key);
    if (it != message_handlers_.end()) {
        return Result<void>(ErrorCode::HANDLER_ALREADY_REGISTERED, 
            "Handler already registered for category " + 
            std::to_string(category) + " and id " + std::to_string(id));
    }
    
    message_handlers_[key] = std::move(handler);
    
    Logger::debug("Registered message handler for category {} and id {}", 
                 category, id);
    
    return Result<void>();
}

Result<void> BaseService::registerModule(std::shared_ptr<ModuleInterface> module) {
    if (!module) {
        return Result<void>(ErrorCode::INVALID_ARGUMENT, "Module cannot be null");
    }
    
    std::string module_name = module->getName();
    
    return registerModuleWithName(module_name, module);
}

Result<void> BaseService::registerModuleWithName(
    const std::string& name, 
    std::shared_ptr<ModuleInterface> module) {
    
    if (name.empty()) {
        return Result<void>(ErrorCode::INVALID_ARGUMENT, "Module name cannot be empty");
    }
    
    if (!module) {
        return Result<void>(ErrorCode::INVALID_ARGUMENT, "Module cannot be null");
    }
    
    std::lock_guard<std::mutex> lock(modules_mutex_);
    
    auto it = modules_.find(name);
    if (it != modules_.end()) {
        return Result<void>(ErrorCode::MODULE_ALREADY_REGISTERED, 
            "Module already registered with name: " + name);
    }
    
    modules_[name] = module;
    
    Logger::info("Registered module: {}", name);
    
    // 初始化模块
    auto init_result = module->init();
    if (init_result.has_error()) {
        // 初始化失败，移除模块
        modules_.erase(name);
        return Result<void>(
            init_result.getError().getCode(),
            "Failed to initialize module: " + name + 
            ", error: " + init_result.getError().getMessage()
        );
    }
    
    // 如果服务已经在运行，则启动模块
    if (running_) {
        auto start_result = module->start();
        if (start_result.has_error()) {
            // 启动失败，停止并移除模块
            module->stop();
            modules_.erase(name);
            return Result<void>(
                start_result.getError().getCode(),
                "Failed to start module: " + name + 
                ", error: " + start_result.getError().getMessage()
            );
        }
    }
    
    return Result<void>();
}

std::shared_ptr<ModuleInterface> BaseService::getModule(const std::string& name) {
    std::lock_guard<std::mutex> lock(modules_mutex_);
    
    auto it = modules_.find(name);
    if (it != modules_.end()) {
        return it->second;
    }
    
    return nullptr;
}

std::string BaseService::getName() const {
    return name_;
}

bool BaseService::isRunning() const {
    return running_;
}

// 扩展功能：启动所有模块
Result<void> BaseService::startAllModules() {
    if (!running_) {
        return Result<void>(ErrorCode::SERVICE_NOT_STARTED, "Service not started");
    }
    
    std::lock_guard<std::mutex> lock(modules_mutex_);
    
    for (auto& pair : modules_) {
        auto& module = pair.second;
        auto result = module->start();
        if (result.has_error()) {
            Logger::error("Failed to start module: {}, error: {}", 
                        pair.first, result.getError().getMessage());
            // 继续启动其他模块，不中断过程
        }
    }
    
    return Result<void>();
}

// 扩展功能：停止所有模块
Result<void> BaseService::stopAllModules() {
    std::lock_guard<std::mutex> lock(modules_mutex_);
    
    for (auto& pair : modules_) {
        auto& module = pair.second;
        auto result = module->stop();
        if (result.has_error()) {
            Logger::error("Failed to stop module: {}, error: {}", 
                        pair.first, result.getError().getMessage());
            // 继续停止其他模块，不中断过程
        }
    }
    
    return Result<void>();
}

// 扩展功能：更新所有模块
Result<void> BaseService::updateAllModules(u64 elapsed_ms) {
    std::lock_guard<std::mutex> lock(modules_mutex_);
    
    for (auto& pair : modules_) {
        auto& module = pair.second;
        auto result = module->update(elapsed_ms);
        if (result.has_error()) {
            Logger::warning("Error updating module: {}, error: {}", 
                          pair.first, result.getError().getMessage());
            // 继续更新其他模块，不中断过程
        }
    }
    
    return Result<void>();
}

// 扩展功能：获取所有模块
std::vector<std::shared_ptr<ModuleInterface>> BaseService::getAllModules() const {
    std::vector<std::shared_ptr<ModuleInterface>> result;
    
    std::lock_guard<std::mutex> lock(modules_mutex_);
    
    result.reserve(modules_.size());
    for (const auto& pair : modules_) {
        result.push_back(pair.second);
    }
    
    return result;
}

// 扩展功能：检查模块是否存在
bool BaseService::hasModule(const std::string& name) const {
    std::lock_guard<std::mutex> lock(modules_mutex_);
    return modules_.find(name) != modules_.end();
}

// 扩展功能：移除模块
Result<void> BaseService::removeModule(const std::string& name) {
    std::lock_guard<std::mutex> lock(modules_mutex_);
    
    auto it = modules_.find(name);
    if (it == modules_.end()) {
        return Result<void>(ErrorCode::MODULE_NOT_FOUND, "Module not found: " + name);
    }
    
    auto module = it->second;
    
    // 如果模块在运行，先停止它
    if (running_) {
        module->stop();
    }
    
    // 移除模块
    modules_.erase(it);
    
    Logger::info("Removed module: {}", name);
    
    return Result<void>();
}

// 辅助函数实现
u32 BaseService::makeHandlerKey(MessageCategoryType category, MessageIdType id) {
    return (static_cast<u32>(category) << 16) | static_cast<u32>(id);
}

} // namespace next_gen
