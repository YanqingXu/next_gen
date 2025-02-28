#ifndef NEXT_GEN_MODULE_INTERFACE_H
#define NEXT_GEN_MODULE_INTERFACE_H

#include <string>
#include <memory>
#include "../utils/error.h"

namespace next_gen {

// Forward declaration
class Service;
class Message;

// Module interface - only contains pure virtual methods
class NEXT_GEN_API ModuleInterface {
public:
    virtual ~ModuleInterface() = default;
    
    // Get module name
    virtual std::string getName() const = 0;
    
    // Initialize module
    virtual Result<void> init() = 0;
    
    // Start module
    virtual Result<void> start() = 0;
    
    // Stop module
    virtual Result<void> stop() = 0;
    
    // Handle message
    virtual Result<void> handleMessage(const Message& message) = 0;
};

} // namespace next_gen

#endif // NEXT_GEN_MODULE_INTERFACE_H
