#include "../../include/utils/logger.h"

namespace next_gen {

// ConsoleSink implementation
void ConsoleSink::log(const LogRecord& record) {
    std::stringstream ss;
    
    // Format time
    auto time_t = std::chrono::system_clock::to_time_t(record.time);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        record.time.time_since_epoch() % std::chrono::seconds(1)).count();
    
    // Use localtime_s instead of localtime for thread safety
    std::tm tm_info;
    localtime_s(&tm_info, &time_t);
    ss << std::put_time(&tm_info, "%Y-%m-%d %H:%M:%S") 
       << "." << std::setfill('0') << std::setw(3) << ms << " ";
    
    // Add log level
    ss << "[" << logLevelToString(record.level) << "] ";
    
    // Add thread ID
    ss << "[" << record.thread_id << "] ";
    
    // Add file and line number
    if (!record.file.empty()) {
        ss << "[" << record.file << ":" << record.line << "] ";
    }
    
    // Add function name
    if (!record.function.empty()) {
        ss << "[" << record.function << "] ";
    }
    
    // Add message
    ss << record.message;
    
    // Output to console
    if (record.level >= LogLevel::ERROR) {
        std::cerr << ss.str() << std::endl;
    } else {
        std::cout << ss.str() << std::endl;
    }
}

// FileSink implementation
FileSink::FileSink(const std::string& filename) : filename_(filename) {
    file_.open(filename, std::ios::out | std::ios::app);
    if (!file_.is_open()) {
        throw std::runtime_error("Failed to open log file: " + filename);
    }
}

FileSink::~FileSink() {
    if (file_.is_open()) {
        file_.close();
    }
}

void FileSink::log(const LogRecord& record) {
    if (!file_.is_open()) {
        return;
    }
    
    std::stringstream ss;
    
    // Format time
    auto time_t = std::chrono::system_clock::to_time_t(record.time);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        record.time.time_since_epoch() % std::chrono::seconds(1)).count();
    std::tm tm_info;
    localtime_s(&tm_info, &time_t);
    ss << std::put_time(&tm_info, "%Y-%m-%d %H:%M:%S") 
       << "." << std::setfill('0') << std::setw(3) << ms << " ";
    
    // Add log level
    ss << "[" << logLevelToString(record.level) << "] ";
    
    // Add thread ID
    ss << "[" << record.thread_id << "] ";
    
    // Add file and line number
    if (!record.file.empty()) {
        ss << "[" << record.file << ":" << record.line << "] ";
    }
    
    // Add function name
    if (!record.function.empty()) {
        ss << "[" << record.function << "] ";
    }
    
    // Add message
    ss << record.message;
    
    // Output to file
    file_ << ss.str() << std::endl;
    file_.flush();
}

// Logger implementation
Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

void Logger::addSink(std::shared_ptr<LogSink> sink) {
    std::lock_guard<std::mutex> lock(mutex_);
    sinks_.push_back(sink);
}

void Logger::init(const std::string& filename, LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    sinks_.clear();
    addSink(std::make_shared<ConsoleSink>());
    addSink(std::make_shared<FileSink>(filename));
    level_ = level;
}

void Logger::setLevel(LogLevel level) {
    level_ = level;
}

LogLevel Logger::getLevel() const {
    return level_;
}

void Logger::log(LogLevel level, const std::string& message, 
                 const std::string& file, int line, 
                 const std::string& function) {
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

void Logger::trace(const std::string& message, 
                   const std::string& file, int line, 
                   const std::string& function) {
    log(LogLevel::TRACE, message, file, line, function);
}

void Logger::debug(const std::string& message, 
                   const std::string& file, int line, 
                   const std::string& function) {
    log(LogLevel::DEBUG, message, file, line, function);
}

void Logger::info(const std::string& message, 
                  const std::string& file, int line, 
                  const std::string& function) {
    log(LogLevel::INFO, message, file, line, function);
}

void Logger::warning(const std::string& message, 
                     const std::string& file, int line, 
                     const std::string& function) {
    log(LogLevel::WARNING, message, file, line, function);
}

void Logger::error(const std::string& message, 
                   const std::string& file, int line, 
                   const std::string& function) {
    log(LogLevel::ERROR, message, file, line, function);
}

void Logger::fatal(const std::string& message, 
                   const std::string& file, int line, 
                   const std::string& function) {
    log(LogLevel::FATAL, message, file, line, function);
}

} // namespace next_gen
