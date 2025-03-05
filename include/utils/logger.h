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
#include <thread>
#include "../core/config.h"

namespace next_gen {

// Log levels
enum class LogLevel {
    TRACE,
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL
};

// Convert log level to string
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

// Log record
struct LogRecord {
    std::chrono::system_clock::time_point time;
    LogLevel level;
    std::string message;
    std::string file;
    int line;
    std::string function;
    std::thread::id thread_id;
};

// Log sink interface
class LogSink {
public:
    virtual ~LogSink() = default;
    virtual void log(const LogRecord& record) = 0;
};

// Console log sink
class ConsoleSink : public LogSink {
public:
    void log(const LogRecord& record) override;
};

// File log sink
class FileSink : public LogSink {
public:
    FileSink(const std::string& filename);
    ~FileSink();
    
    void log(const LogRecord& record) override;
    
private:
    std::string filename_;
    std::ofstream file_;
};

// Logger manager
class NEXT_GEN_API Logger {
public:
    static Logger& instance();
    
    // Add log sink
    void addSink(std::shared_ptr<LogSink> sink);
    
    // Initialize logger with file
    void init(const std::string& filename, LogLevel level = LogLevel::INFO);
    
    // Set log level
    void setLevel(LogLevel level);
    
    // Get log level
    LogLevel getLevel() const;
    
    // Log message
    void log(LogLevel level, const std::string& message, 
             const std::string& file = "", int line = 0, 
             const std::string& function = "");
    
    // Convenience logging methods
    void trace(const std::string& message, 
               const std::string& file = "", int line = 0, 
               const std::string& function = "");
    
    void debug(const std::string& message, 
               const std::string& file = "", int line = 0, 
               const std::string& function = "");
    
    void info(const std::string& message, 
              const std::string& file = "", int line = 0, 
              const std::string& function = "");
    
    void warning(const std::string& message, 
                 const std::string& file = "", int line = 0, 
                 const std::string& function = "");
    
    void error(const std::string& message, 
               const std::string& file = "", int line = 0, 
               const std::string& function = "");
    
    void fatal(const std::string& message, 
               const std::string& file = "", int line = 0, 
               const std::string& function = "");
    
private:
    Logger() {
        // Add console sink by default
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

// Convenience logging macros
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
