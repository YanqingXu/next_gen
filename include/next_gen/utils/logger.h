#ifndef NEXT_GEN_LOGGER_H
#define NEXT_GEN_LOGGER_H

#include <string>
#include <memory>
#include <mutex>
#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <functional>
#include "../core/config.h"

namespace next_gen {

// 日志级别
enum class LogLevel {
    TRACE,
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL
};

// 日志级别转字符串
inline const char* logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

// 日志记录
struct LogRecord {
    std::chrono::system_clock::time_point time;
    LogLevel level;
    std::string message;
    std::string file;
    int line;
    std::string function;
    std::thread::id thread_id;
};

// 日志接收器接口
class LogSink {
public:
    virtual ~LogSink() = default;
    virtual void log(const LogRecord& record) = 0;
};

// 控制台日志接收器
class ConsoleSink : public LogSink {
public:
    void log(const LogRecord& record) override {
        std::stringstream ss;
        
        // 格式化时间
        auto time_t = std::chrono::system_clock::to_time_t(record.time);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            record.time.time_since_epoch() % std::chrono::seconds(1)).count();
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") 
           << "." << std::setfill('0') << std::setw(3) << ms << " ";
        
        // 添加日志级别
        ss << "[" << logLevelToString(record.level) << "] ";
        
        // 添加线程ID
        ss << "[" << record.thread_id << "] ";
        
        // 添加文件和行号
        if (!record.file.empty()) {
            ss << "[" << record.file << ":" << record.line << "] ";
        }
        
        // 添加函数名
        if (!record.function.empty()) {
            ss << "[" << record.function << "] ";
        }
        
        // 添加消息
        ss << record.message;
        
        // 输出到控制台
        if (record.level >= LogLevel::ERROR) {
            std::cerr << ss.str() << std::endl;
        } else {
            std::cout << ss.str() << std::endl;
        }
    }
};

// 文件日志接收器
class FileSink : public LogSink {
public:
    FileSink(const std::string& filename) : filename_(filename) {
        file_.open(filename, std::ios::out | std::ios::app);
        if (!file_.is_open()) {
            throw std::runtime_error("Failed to open log file: " + filename);
        }
    }
    
    ~FileSink() {
        if (file_.is_open()) {
            file_.close();
        }
    }
    
    void log(const LogRecord& record) override {
        if (!file_.is_open()) {
            return;
        }
        
        std::stringstream ss;
        
        // 格式化时间
        auto time_t = std::chrono::system_clock::to_time_t(record.time);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            record.time.time_since_epoch() % std::chrono::seconds(1)).count();
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") 
           << "." << std::setfill('0') << std::setw(3) << ms << " ";
        
        // 添加日志级别
        ss << "[" << logLevelToString(record.level) << "] ";
        
        // 添加线程ID
        ss << "[" << record.thread_id << "] ";
        
        // 添加文件和行号
        if (!record.file.empty()) {
            ss << "[" << record.file << ":" << record.line << "] ";
        }
        
        // 添加函数名
        if (!record.function.empty()) {
            ss << "[" << record.function << "] ";
        }
        
        // 添加消息
        ss << record.message;
        
        // 输出到文件
        file_ << ss.str() << std::endl;
        file_.flush();
    }
    
private:
    std::string filename_;
    std::ofstream file_;
};

// 日志管理器
class NEXT_GEN_API Logger {
public:
    static Logger& instance() {
        static Logger instance;
        return instance;
    }
    
    // 添加日志接收器
    void addSink(std::shared_ptr<LogSink> sink) {
        std::lock_guard<std::mutex> lock(mutex_);
        sinks_.push_back(sink);
    }
    
    // 设置日志级别
    void setLevel(LogLevel level) {
        level_ = level;
    }
    
    // 获取日志级别
    LogLevel getLevel() const {
        return level_;
    }
    
    // 记录日志
    void log(LogLevel level, const std::string& message, 
             const std::string& file = "", int line = 0, 
             const std::string& function = "") {
        if (level < level_) {
            return;
        }
        
        LogRecord record;
        record.time = std::chrono::system_clock::now();
        record.level = level;
        record.message = message;
        record.file = file;
        record.line = line;
        record.function = function;
        record.thread_id = std::this_thread::get_id();
        
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& sink : sinks_) {
            sink->log(record);
        }
    }
    
    // 便捷日志方法
    void trace(const std::string& message, 
               const std::string& file = "", int line = 0, 
               const std::string& function = "") {
        log(LogLevel::TRACE, message, file, line, function);
    }
    
    void debug(const std::string& message, 
               const std::string& file = "", int line = 0, 
               const std::string& function = "") {
        log(LogLevel::DEBUG, message, file, line, function);
    }
    
    void info(const std::string& message, 
              const std::string& file = "", int line = 0, 
              const std::string& function = "") {
        log(LogLevel::INFO, message, file, line, function);
    }
    
    void warning(const std::string& message, 
                 const std::string& file = "", int line = 0, 
                 const std::string& function = "") {
        log(LogLevel::WARNING, message, file, line, function);
    }
    
    void error(const std::string& message, 
               const std::string& file = "", int line = 0, 
               const std::string& function = "") {
        log(LogLevel::ERROR, message, file, line, function);
    }
    
    void fatal(const std::string& message, 
               const std::string& file = "", int line = 0, 
               const std::string& function = "") {
        log(LogLevel::FATAL, message, file, line, function);
    }
    
private:
    Logger() {
        // 默认添加控制台接收器
        addSink(std::make_shared<ConsoleSink>());
        level_ = LogLevel::INFO;
    }
    
    ~Logger() = default;
    
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    std::vector<std::shared_ptr<LogSink>> sinks_;
    std::mutex mutex_;
    LogLevel level_;
};

} // namespace next_gen

// 便捷宏
#define NEXT_GEN_LOG_TRACE(message) \
    next_gen::Logger::instance().trace(message, __FILE__, __LINE__, __FUNCTION__)

#define NEXT_GEN_LOG_DEBUG(message) \
    next_gen::Logger::instance().debug(message, __FILE__, __LINE__, __FUNCTION__)

#define NEXT_GEN_LOG_INFO(message) \
    next_gen::Logger::instance().info(message, __FILE__, __LINE__, __FUNCTION__)

#define NEXT_GEN_LOG_WARNING(message) \
    next_gen::Logger::instance().warning(message, __FILE__, __LINE__, __FUNCTION__)

#define NEXT_GEN_LOG_ERROR(message) \
    next_gen::Logger::instance().error(message, __FILE__, __LINE__, __FUNCTION__)

#define NEXT_GEN_LOG_FATAL(message) \
    next_gen::Logger::instance().fatal(message, __FILE__, __LINE__, __FUNCTION__)

#endif // NEXT_GEN_LOGGER_H
