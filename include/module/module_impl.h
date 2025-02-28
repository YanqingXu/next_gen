#ifndef NEXT_GEN_MODULE_IMPL_H
#define NEXT_GEN_MODULE_IMPL_H

#include "../core/service.h"
#include "module.h"

namespace next_gen {

// Implementation of module methods that require Service class definition

// Get service
inline std::shared_ptr<Service> Module::getService() {
    return service_.lock();
}

// Post message
inline Result<void> Module::postMessage(std::unique_ptr<Message> message) {
    auto service = getService();
    if (!service) {
        return Result<void>(ErrorCode::SERVICE_NOT_AVAILABLE, "Service is not available");
    }
    return service->postMessage(std::move(message));
}

} // namespace next_gen

#endif // NEXT_GEN_MODULE_IMPL_H
