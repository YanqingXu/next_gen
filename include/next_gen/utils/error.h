#ifndef NEXT_GEN_ERROR_H
#define NEXT_GEN_ERROR_H

#include <string>
#include <exception>
#include <system_error>
#include <memory>
#include "../core/config.h"

namespace next_gen {

// 错误代码枚举
enum class ErrorCode {
    // 通用错误
    SUCCESS = 0,
    UNKNOWN_ERROR,
    NOT_IMPLEMENTED,
    INVALID_ARGUMENT,
    OUT_OF_RANGE,
    
    // 系统错误
    SYSTEM_ERROR,
    
    // 网络错误
    NETWORK_ERROR,
    CONNECTION_FAILED,
    CONNECTION_CLOSED,
    TIMEOUT,
    
    // 消息错误
    MESSAGE_ERROR,
    INVALID_MESSAGE,
    MESSAGE_TOO_LARGE,
    
    // 服务错误
    SERVICE_ERROR,
    SERVICE_NOT_FOUND,
    SERVICE_ALREADY_EXISTS,
    SERVICE_NOT_STARTED,
    SERVICE_ALREADY_STARTED,
    
    // 模块错误
    MODULE_ERROR,
    MODULE_NOT_FOUND,
    MODULE_ALREADY_EXISTS,
    MODULE_INITIALIZATION_FAILED
};

// 错误类别
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
            case ErrorCode::MODULE_ERROR: return "Module error";
            case ErrorCode::MODULE_NOT_FOUND: return "Module not found";
            case ErrorCode::MODULE_ALREADY_EXISTS: return "Module already exists";
            case ErrorCode::MODULE_INITIALIZATION_FAILED: return "Module initialization failed";
            default: return "Unknown error code";
        }
    }
};

// 创建错误代码
inline std::error_code make_error_code(ErrorCode e) {
    return {static_cast<int>(e), ErrorCategory::instance()};
}

// 错误异常类
class NEXT_GEN_API Error : public std::system_error {
public:
    Error(ErrorCode code, const std::string& what_arg)
        : std::system_error(make_error_code(code), what_arg) {}
    
    Error(ErrorCode code)
        : std::system_error(make_error_code(code)) {}
    
    ErrorCode code() const {
        return static_cast<ErrorCode>(std::system_error::code().value());
    }
};

// 结果类模板，用于返回值或错误
template<typename T>
class Result {
public:
    // 成功构造函数
    Result(const T& value) : value_(new T(value)), has_error_(false) {}
    Result(T&& value) : value_(new T(std::move(value))), has_error_(false) {}
    
    // 错误构造函数
    Result(ErrorCode code, const std::string& what_arg)
        : error_(new Error(code, what_arg)), has_error_(true) {}
    
    Result(ErrorCode code)
        : error_(new Error(code)), has_error_(true) {}
    
    Result(const Error& error)
        : error_(new Error(error)), has_error_(true) {}
    
    // 检查是否有错误
    bool has_error() const { return has_error_; }
    
    // 获取值，如果有错误则抛出异常
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
    
    // 获取错误，如果没有错误则返回成功
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

// 特化的void结果类，用于不返回值的操作
template<>
class Result<void> {
public:
    // 成功构造函数
    Result() : has_error_(false) {}
    
    // 错误构造函数
    Result(ErrorCode code, const std::string& what_arg)
        : error_(new Error(code, what_arg)), has_error_(true) {}
    
    Result(ErrorCode code)
        : error_(new Error(code)), has_error_(true) {}
    
    Result(const Error& error)
        : error_(new Error(error)), has_error_(true) {}
    
    // 检查是否有错误
    bool has_error() const { return has_error_; }
    
    // 获取错误，如果没有错误则返回成功
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

// 使std::error_code能够处理我们的ErrorCode
namespace std {
    template <>
    struct is_error_code_enum<next_gen::ErrorCode> : true_type {};
}

#endif // NEXT_GEN_ERROR_H
