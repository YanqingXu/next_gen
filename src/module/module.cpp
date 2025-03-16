#include "../../include/module/module.h"
#include "../../include/core/service.h"
#include "../../include/utils/logger.h"

namespace next_gen {

// 发送消息到服务
Result<void> Module::postMessage(std::unique_ptr<Message> message) {
    auto service = service_.lock();
    if (!service) {
        return Result<void>(ErrorCode::SERVICE_ERROR, "Service not available");
    }
    
    return service->postMessage(std::move(message));
}

// 获取服务实例
std::shared_ptr<Service> Module::getService() {
    return service_.lock();
}

// 添加模块依赖关系管理
class ModuleDependencyManager {
public:
    static ModuleDependencyManager& instance() {
        static ModuleDependencyManager instance;
        return instance;
    }
    
    // 添加模块依赖关系
    void addDependency(const std::string& module, const std::string& dependency) {
        std::lock_guard<std::mutex> lock(mutex_);
        dependencies_[module].push_back(dependency);
    }
    
    // 检查是否有循环依赖
    bool hasCircularDependency(const std::string& module) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::set<std::string> visited;
        return hasCircularDependencyImpl(module, visited);
    }
    
    // 获取模块的所有依赖
    std::vector<std::string> getDependencies(const std::string& module) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = dependencies_.find(module);
        if (it != dependencies_.end()) {
            return it->second;
        }
        return {};
    }
    
    // 清除模块的依赖
    void clearDependencies(const std::string& module) {
        std::lock_guard<std::mutex> lock(mutex_);
        dependencies_.erase(module);
    }
    
private:
    ModuleDependencyManager() {}
    
    // 递归检查循环依赖
    bool hasCircularDependencyImpl(const std::string& module, std::set<std::string>& visited) {
        if (visited.find(module) != visited.end()) {
            return true; // 循环依赖
        }
        
        visited.insert(module);
        
        auto it = dependencies_.find(module);
        if (it != dependencies_.end()) {
            for (const auto& dep : it->second) {
                if (hasCircularDependencyImpl(dep, visited)) {
                    return true;
                }
            }
        }
        
        visited.erase(module);
        return false;
    }
    
    std::unordered_map<std::string, std::vector<std::string>> dependencies_;
    std::mutex mutex_;
};

// 扩展Module类的功能，添加依赖管理

// 添加模块依赖
Result<void> Module::addDependency(const std::string& dependency_name) {
    auto service = service_.lock();
    if (!service) {
        return Result<void>(ErrorCode::SERVICE_ERROR, "Service not available");
    }
    
    // 检查依赖模块是否存在
    auto dependency = service->getModule(dependency_name);
    if (!dependency) {
        return Result<void>(ErrorCode::MODULE_NOT_FOUND, 
                          "Dependency module not found: " + dependency_name);
    }
    
    // 添加依赖关系
    ModuleDependencyManager::instance().addDependency(getName(), dependency_name);
    
    // 检查是否造成循环依赖
    if (ModuleDependencyManager::instance().hasCircularDependency(getName())) {
        // 移除刚添加的依赖
        ModuleDependencyManager::instance().clearDependencies(getName());
        return Result<void>(ErrorCode::CIRCULAR_DEPENDENCY, 
                          "Adding dependency would create circular dependency");
    }
    
    Logger::debug("Added dependency: {} -> {}", getName(), dependency_name);
    
    return Result<void>();
}

// 检查模块是否依赖于另一个模块
bool Module::dependsOn(const std::string& module_name) {
    auto dependencies = ModuleDependencyManager::instance().getDependencies(getName());
    return std::find(dependencies.begin(), dependencies.end(), module_name) != dependencies.end();
}

// 获取模块的所有依赖
std::vector<std::string> Module::getDependencies() {
    return ModuleDependencyManager::instance().getDependencies(getName());
}

// 注册模块事件回调
struct ModuleEventHandlers {
    std::function<void(std::shared_ptr<ModuleInterface>)> on_init;
    std::function<void(std::shared_ptr<ModuleInterface>)> on_start;
    std::function<void(std::shared_ptr<ModuleInterface>)> on_stop;
    std::function<void(std::shared_ptr<ModuleInterface>, u64)> on_update;
};

// 模块事件管理器
class ModuleEventManager {
public:
    static ModuleEventManager& instance() {
        static ModuleEventManager instance;
        return instance;
    }
    
    void registerEventHandlers(const std::string& module_name, 
                             ModuleEventHandlers handlers) {
        std::lock_guard<std::mutex> lock(mutex_);
        event_handlers_[module_name] = handlers;
    }
    
    void removeEventHandlers(const std::string& module_name) {
        std::lock_guard<std::mutex> lock(mutex_);
        event_handlers_.erase(module_name);
    }
    
    void triggerInitEvent(std::shared_ptr<ModuleInterface> module) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = event_handlers_.find(module->getName());
        if (it != event_handlers_.end() && it->second.on_init) {
            it->second.on_init(module);
        }
    }
    
    void triggerStartEvent(std::shared_ptr<ModuleInterface> module) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = event_handlers_.find(module->getName());
        if (it != event_handlers_.end() && it->second.on_start) {
            it->second.on_start(module);
        }
    }
    
    void triggerStopEvent(std::shared_ptr<ModuleInterface> module) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = event_handlers_.find(module->getName());
        if (it != event_handlers_.end() && it->second.on_stop) {
            it->second.on_stop(module);
        }
    }
    
    void triggerUpdateEvent(std::shared_ptr<ModuleInterface> module, u64 elapsed_ms) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = event_handlers_.find(module->getName());
        if (it != event_handlers_.end() && it->second.on_update) {
            it->second.on_update(module, elapsed_ms);
        }
    }
    
private:
    ModuleEventManager() {}
    
    std::unordered_map<std::string, ModuleEventHandlers> event_handlers_;
    std::mutex mutex_;
};

// 增强模块工厂类，添加更多功能
Result<std::shared_ptr<ModuleInterface>> ModuleFactory::createAndRegisterModule(
    std::shared_ptr<Service> service, 
    const std::string& module_name, 
    std::function<std::shared_ptr<ModuleInterface>()> factory_func) {
    
    if (!service) {
        return Result<std::shared_ptr<ModuleInterface>>(
            ErrorCode::SERVICE_ERROR, "Service not available");
    }
    
    // 检查模块是否已存在
    if (service->getModule(module_name)) {
        return Result<std::shared_ptr<ModuleInterface>>(
            ErrorCode::MODULE_ALREADY_REGISTERED, 
            "Module already registered: " + module_name);
    }
    
    // 创建模块实例
    auto module = factory_func();
    if (!module) {
        return Result<std::shared_ptr<ModuleInterface>>(
            ErrorCode::MODULE_CREATION_FAILED, 
            "Failed to create module: " + module_name);
    }
    
    // 注册模块
    auto result = service->registerModule(module);
    if (result.has_error()) {
        return Result<std::shared_ptr<ModuleInterface>>(
            result.getError().getCode(), 
            result.getError().getMessage());
    }
    
    return Result<std::shared_ptr<ModuleInterface>>(module);
}

// 设置模块事件处理器
void ModuleFactory::setModuleEventHandlers(
    const std::string& module_name, 
    std::function<void(std::shared_ptr<ModuleInterface>)> on_init,
    std::function<void(std::shared_ptr<ModuleInterface>)> on_start,
    std::function<void(std::shared_ptr<ModuleInterface>)> on_stop,
    std::function<void(std::shared_ptr<ModuleInterface>, u64)> on_update) {
    
    ModuleEventHandlers handlers = {
        on_init,
        on_start,
        on_stop,
        on_update
    };
    
    ModuleEventManager::instance().registerEventHandlers(module_name, handlers);
}

// 模块热加载/卸载支持
class ModuleHotSwapManager {
public:
    static ModuleHotSwapManager& instance() {
        static ModuleHotSwapManager instance;
        return instance;
    }
    
    // 保存模块状态
    void saveModuleState(const std::string& module_name, const std::string& state) {
        std::lock_guard<std::mutex> lock(mutex_);
        module_states_[module_name] = state;
    }
    
    // 获取模块状态
    std::string getModuleState(const std::string& module_name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = module_states_.find(module_name);
        if (it != module_states_.end()) {
            return it->second;
        }
        return "";
    }
    
    // 注册状态转换处理函数
    void registerStateTransformer(
        const std::string& module_name,
        std::function<std::string(std::shared_ptr<ModuleInterface>)> state_getter,
        std::function<void(std::shared_ptr<ModuleInterface>, const std::string&)> state_setter) {
        
        std::lock_guard<std::mutex> lock(mutex_);
        state_transformers_[module_name] = {state_getter, state_setter};
    }
    
    // 获取模块状态
    std::string captureModuleState(std::shared_ptr<ModuleInterface> module) {
        if (!module) return "";
        
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = state_transformers_.find(module->getName());
        if (it != state_transformers_.end() && it->second.first) {
            return it->second.first(module);
        }
        return "";
    }
    
    // 设置模块状态
    void restoreModuleState(std::shared_ptr<ModuleInterface> module, const std::string& state) {
        if (!module || state.empty()) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = state_transformers_.find(module->getName());
        if (it != state_transformers_.end() && it->second.second) {
            it->second.second(module, state);
        }
    }
    
    // 热更新模块
    Result<std::shared_ptr<ModuleInterface>> hotSwapModule(
        std::shared_ptr<Service> service,
        const std::string& module_name,
        std::function<std::shared_ptr<ModuleInterface>()> factory_func) {
        
        if (!service) {
            return Result<std::shared_ptr<ModuleInterface>>(
                ErrorCode::SERVICE_ERROR, "Service not available");
        }
        
        // 获取原模块
        auto old_module = service->getModule(module_name);
        if (!old_module) {
            return Result<std::shared_ptr<ModuleInterface>>(
                ErrorCode::MODULE_NOT_FOUND, "Module not found: " + module_name);
        }
        
        // 捕获模块状态
        std::string state = captureModuleState(old_module);
        
        // 创建新模块
        auto new_module = factory_func();
        if (!new_module) {
            return Result<std::shared_ptr<ModuleInterface>>(
                ErrorCode::MODULE_CREATION_FAILED, 
                "Failed to create new module: " + module_name);
        }
        
        // 停止旧模块
        old_module->stop();
        
        // 移除旧模块
        service->removeModule(module_name);
        
        // 注册新模块
        auto result = service->registerModule(new_module);
        if (result.has_error()) {
            return Result<std::shared_ptr<ModuleInterface>>(
                result.getError().getCode(), 
                result.getError().getMessage());
        }
        
        // 恢复模块状态
        restoreModuleState(new_module, state);
        
        // 如果服务正在运行，启动新模块
        if (service->isRunning()) {
            new_module->start();
        }
        
        return Result<std::shared_ptr<ModuleInterface>>(new_module);
    }
    
private:
    ModuleHotSwapManager() {}
    
    std::unordered_map<std::string, std::string> module_states_;
    std::unordered_map<std::string, 
        std::pair<
            std::function<std::string(std::shared_ptr<ModuleInterface>)>,
            std::function<void(std::shared_ptr<ModuleInterface>, const std::string&)>
        >> state_transformers_;
    std::mutex mutex_;
};

} // namespace next_gen
