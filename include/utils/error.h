#ifndef NEXT_GEN_ERROR_H
#define NEXT_GEN_ERROR_H

#include <string>
#include <exception>
#include <system_error>
#include <memory>
#include "../core/config.h"

namespace next_gen {

// Error code enumeration
enum class ErrorCode {
    // General errors
    SUCCESS = 0,
    UNKNOWN_ERROR,
    NOT_IMPLEMENTED,
    INVALID_ARGUMENT,
    OUT_OF_RANGE,
    
    // System errors
    SYSTEM_ERROR,
    
    // Network errors
    NETWORK_ERROR,
    CONNECTION_FAILED,
    CONNECTION_CLOSED,
    TIMEOUT,
    
    // Message errors
    MESSAGE_ERROR,
    INVALID_MESSAGE,
    MESSAGE_TOO_LARGE,
    
    // Service errors
    SERVICE_ERROR,
    SERVICE_NOT_FOUND,
    SERVICE_ALREADY_EXISTS,
    SERVICE_NOT_STARTED,
    SERVICE_ALREADY_STARTED,
    
    // Session errors
    SESSION_ERROR,
    SESSION_NOT_FOUND,
    SESSION_ALREADY_EXISTS,
    SESSION_CLOSED,
    
    // Module errors
    MODULE_ERROR,
    MODULE_NOT_FOUND,
    MODULE_ALREADY_EXISTS,
    MODULE_INITIALIZATION_FAILED
};

// Error category
class NEXT_GEN_API ErrorCategory : public std::error_category {
public:
    static const ErrorCategory& instance() {
        static ErrorCategory instance;
        return instance;
    }
    
    const char* name() const noexcept override {
        return "next_gen";
    }
    
    std::string message(int ev) const override {
        switch (static_cast<ErrorCode>(ev)) {
            case ErrorCode::SUCCESS: return "Success";
            case ErrorCode::UNKNOWN_ERROR: return "Unknown error";
            case ErrorCode::NOT_IMPLEMENTED: return "Not implemented";
            case ErrorCode::INVALID_ARGUMENT: return "Invalid argument";
            case ErrorCode::OUT_OF_RANGE: return "Out of range";
            case ErrorCode::SYSTEM_ERROR: return "System error";
            case ErrorCode::NETWORK_ERROR: return "Network error";
            case ErrorCode::CONNECTION_FAILED: return "Connection failed";
            case ErrorCode::CONNECTION_CLOSED: return "Connection closed";
            case ErrorCode::TIMEOUT: return "Timeout";
            case ErrorCode::MESSAGE_ERROR: return "Message error";
            case ErrorCode::INVALID_MESSAGE: return "Invalid message";
            case ErrorCode::MESSAGE_TOO_LARGE: return "Message too large";
            case ErrorCode::SERVICE_ERROR: return "Service error";
            case ErrorCode::SERVICE_NOT_FOUND: return "Service not found";
            case ErrorCode::SERVICE_ALREADY_EXISTS: return "Service already exists";
            case ErrorCode::SERVICE_NOT_STARTED: return "Service not started";
            case ErrorCode::SERVICE_ALREADY_STARTED: return "Service already started";
            case ErrorCode::SESSION_ERROR: return "Session error";
            case ErrorCode::SESSION_NOT_FOUND: return "Session not found";
            case ErrorCode::SESSION_ALREADY_EXISTS: return "Session already exists";
            case ErrorCode::SESSION_CLOSED: return "Session closed";
            case ErrorCode::MODULE_ERROR: return "Module error";
            case ErrorCode::MODULE_NOT_FOUND: return "Module not found";
            case ErrorCode::MODULE_ALREADY_EXISTS: return "Module already exists";
            case ErrorCode::MODULE_INITIALIZATION_FAILED: return "Module initialization failed";
            default: return "Unknown error code";
        }
    }
};

// Create error code
inline std::error_code make_error_code(ErrorCode e) {
    return {static_cast<int>(e), ErrorCategory::instance()};
}

// Error exception class
class NEXT_GEN_API Error : public std::system_error {
public:
    Error(ErrorCode code, const std::string& what_arg)
        : std::system_error(make_error_code(code), what_arg) {}
    
    Error(ErrorCode code)
        : std::system_error(make_error_code(code)) {}
    
    ErrorCode code() const {
        return static_cast<ErrorCode>(std::system_error::code().value());
    }
    
    // 添加message方法，返回错误信息
    std::string message() const {
        return what();
    }
};

// Result template class for returning value or error
template<typename T>
class Result {
public:
    // Success constructor
    Result(const T& value) : value_(new T(value)), has_error_(false) {}
    Result(T&& value) : value_(new T(std::move(value))), has_error_(false) {}
    
    // Error constructor
    Result(ErrorCode code, const std::string& what_arg)
        : error_(std::make_unique<Error>(code, what_arg)), has_error_(true), value_(nullptr) {}
    
    Result(ErrorCode code)
        : error_(std::make_unique<Error>(code)), has_error_(true), value_(nullptr) {}
    
    Result(const Error& error)
        : error_(std::make_unique<Error>(error.code(), error.what())), has_error_(true), value_(nullptr) {}
    
    // Check if has error
    bool has_error() const { return has_error_; }
    
    // Get value, throw exception if has error
    const T& value() const {
        if (has_error_) {
            throw *error_;
        }
        return *value_;
    }
    
    T& value() {
        if (has_error_) {
            throw *error_;
        }
        return *value_;
    }
    
    // Get error, return success if no error
    const Error& error() const {
        if (!has_error_) {
            static const Error success(ErrorCode::SUCCESS);
            return success;
        }
        return *error_;
    }
    
private:
    std::unique_ptr<T> value_;
    std::unique_ptr<Error> error_;
    bool has_error_;
};

// Specialized void result class for operations that don't return a value
template<>
class Result<void> {
public:
    // Success constructor
    Result() : has_error_(false) {}
    
    // Error constructor
    Result(ErrorCode code, const std::string& what_arg)
        : error_(std::make_unique<Error>(code, what_arg)), has_error_(true) {}
    
    Result(ErrorCode code)
        : error_(std::make_unique<Error>(code)), has_error_(true) {}
    
    Result(const Error& error)
        : error_(std::make_unique<Error>(error.code(), error.what())), has_error_(true) {}
    
    // Check if has error
    bool has_error() const { return has_error_; }
    
    // Get error, return success if no error
    const Error& error() const {
        if (!has_error_) {
            static const Error success(ErrorCode::SUCCESS);
            return success;
        }
        return *error_;
    }
    
private:
    std::unique_ptr<Error> error_;
    bool has_error_;
};

} // namespace next_gen

// Enable std::error_code to handle our ErrorCode
namespace std {
    template <>
    struct is_error_code_enum<next_gen::ErrorCode> : std::true_type {};
}

#endif // NEXT_GEN_ERROR_H
