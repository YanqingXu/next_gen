#ifndef NEXT_GEN_MODULE_H
#define NEXT_GEN_MODULE_H

#include <string>
#include <memory>
#include "../utils/error.h"
#include "module_interface.h"

namespace next_gen {

// Forward declaration of Service class
class Service;

// Module interface
class NEXT_GEN_API Module : public ModuleInterface, public std::enable_shared_from_this<Module> {
public:
    Module(std::weak_ptr<Service> service) : service_(service) {}
    
    virtual ~Module() = default;
    
    // Get module name
    virtual std::string getName() const = 0;
    
    // Initialize module
    virtual Result<void> init() {
        return Result<void>();
    }
    
    // Start module
    virtual Result<void> start() {
        return Result<void>();
    }
    
    // Stop module
    virtual Result<void> stop() {
        return Result<void>();
    }
    
    // Update module
    virtual Result<void> update(u64 elapsed_ms) {
        return Result<void>();
    }
    
    // Register message handler
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
    
    // Post message
    Result<void> postMessage(std::unique_ptr<Message> message);
    
    // Get service
    std::shared_ptr<Service> getService();
    
protected:
    std::weak_ptr<Service> service_;
};

// Base module implementation
template<typename ModuleType>
class NEXT_GEN_API BaseModule : public Module {
public:
    BaseModule(std::weak_ptr<Service> service) : Module(service) {}
    
    // Get module name
    std::string getName() const override {
        return ModuleType::MODULE_NAME;
    }
    
    // Get module instance
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

// Module factory
class NEXT_GEN_API ModuleFactory {
public:
    // Create and register module
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

// Convenience macro for defining modules
#define NEXT_GEN_DEFINE_MODULE(ModuleName) \
    static constexpr const char* MODULE_NAME = #ModuleName;

#endif // NEXT_GEN_MODULE_H
