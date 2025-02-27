#ifndef NEXT_GEN_NET_SERVICE_H
#define NEXT_GEN_NET_SERVICE_H

#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>
#include "../core/service.h"
#include "../utils/error.h"
#include "../utils/logger.h"
#include "../message/message.h"

namespace next_gen {

// 会话ID类型
using SessionId = u32;

// 会话状态
enum class SessionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    AUTHENTICATING,
    AUTHENTICATED,
    CLOSING
};

// 会话接口
class NEXT_GEN_API Session {
public:
    virtual ~Session() = default;
    
    // 获取会话ID
    virtual SessionId getId() const = 0;
    
    // 获取远程地址
    virtual std::string getRemoteAddress() const = 0;
    
    // 获取会话状态
    virtual SessionState getState() const = 0;
    
    // 发送消息
    virtual Result<void> send(const Message& message) = 0;
    
    // 关闭会话
    virtual Result<void> close() = 0;
    
    // 设置会话属性
    virtual void setAttribute(const std::string& key, const std::string& value) = 0;
    
    // 获取会话属性
    virtual std::string getAttribute(const std::string& key) const = 0;
    
    // 检查会话属性是否存在
    virtual bool hasAttribute(const std::string& key) const = 0;
    
    // 移除会话属性
    virtual void removeAttribute(const std::string& key) = 0;
    
    // 清空所有会话属性
    virtual void clearAttributes() = 0;
};

// 会话事件处理器
class NEXT_GEN_API SessionHandler {
public:
    virtual ~SessionHandler() = default;
    
    // 会话创建事件
    virtual void onSessionCreated(std::shared_ptr<Session> session) {}
    
    // 会话打开事件
    virtual void onSessionOpened(std::shared_ptr<Session> session) {}
    
    // 会话关闭事件
    virtual void onSessionClosed(std::shared_ptr<Session> session) {}
    
    // 会话空闲事件
    virtual void onSessionIdle(std::shared_ptr<Session> session) {}
    
    // 会话异常事件
    virtual void onSessionError(std::shared_ptr<Session> session, const Error& error) {}
    
    // 消息接收事件
    virtual void onMessageReceived(std::shared_ptr<Session> session, std::unique_ptr<Message> message) {}
    
    // 消息发送事件
    virtual void onMessageSent(std::shared_ptr<Session> session, const Message& message) {}
};

// 网络服务配置
struct NetServiceConfig {
    std::string bind_address = "0.0.0.0";     // 绑定地址
    u16 port = 0;                             // 端口
    u32 max_connections = 1000;               // 最大连接数
    u32 read_buffer_size = 8192;              // 读缓冲区大小
    u32 write_buffer_size = 8192;             // 写缓冲区大小
    u32 idle_timeout_ms = 60000;              // 空闲超时时间（毫秒）
    bool tcp_nodelay = true;                  // 是否启用TCP_NODELAY
    bool tcp_keepalive = true;                // 是否启用TCP_KEEPALIVE
    bool reuse_address = true;                // 是否启用SO_REUSEADDR
};

// 网络服务接口
class NEXT_GEN_API NetService : public BaseService {
public:
    NetService(const std::string& name, const NetServiceConfig& config = NetServiceConfig())
        : BaseService(name), config_(config), next_session_id_(1) {}
    
    virtual ~NetService() {
        if (isRunning()) {
            stop();
        }
    }
    
protected:
    // 初始化网络服务
    Result<void> onInit() override {
        NEXT_GEN_LOG_INFO("Initializing network service: " + getName());
        
        // 初始化网络库
        auto result = initNetworkLibrary();
        if (result.has_error()) {
            NEXT_GEN_LOG_ERROR("Failed to initialize network library: " + result.error().message());
            return result;
        }
        
        return Result<void>();
    }
    
    // 启动网络服务
    Result<void> onStart() override {
        NEXT_GEN_LOG_INFO("Starting network service: " + getName() + 
                        ", binding to " + config_.bind_address + ":" + std::to_string(config_.port));
        
        // 启动服务器
        auto result = startServer();
        if (result.has_error()) {
            NEXT_GEN_LOG_ERROR("Failed to start server: " + result.error().message());
            return result;
        }
        
        return Result<void>();
    }
    
    // 停止网络服务
    Result<void> onStop() override {
        NEXT_GEN_LOG_INFO("Stopping network service: " + getName());
        
        // 停止服务器
        auto result = stopServer();
        if (result.has_error()) {
            NEXT_GEN_LOG_ERROR("Failed to stop server: " + result.error().message());
            return result;
        }
        
        // 关闭所有会话
        closeAllSessions();
        
        // 清理网络库
        cleanupNetworkLibrary();
        
        return Result<void>();
    }
    
    // 更新网络服务
    Result<void> onUpdate(u64 elapsed_ms) override {
        // 检查空闲会话
        checkIdleSessions(elapsed_ms);
        
        return Result<void>();
    }
    
    // 初始化网络库（子类实现）
    virtual Result<void> initNetworkLibrary() = 0;
    
    // 清理网络库（子类实现）
    virtual Result<void> cleanupNetworkLibrary() = 0;
    
    // 启动服务器（子类实现）
    virtual Result<void> startServer() = 0;
    
    // 停止服务器（子类实现）
    virtual Result<void> stopServer() = 0;
    
    // 添加会话
    Result<void> addSession(std::shared_ptr<Session> session) {
        if (!session) {
            return Result<void>(ErrorCode::INVALID_ARGUMENT, "Session is null");
        }
        
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        
        SessionId id = session->getId();
        if (sessions_.find(id) != sessions_.end()) {
            return Result<void>(ErrorCode::SESSION_ALREADY_EXISTS, "Session already exists: " + std::to_string(id));
        }
        
        sessions_[id] = session;
        
        // 触发会话创建事件
        if (session_handler_) {
            session_handler_->onSessionCreated(session);
        }
        
        return Result<void>();
    }
    
    // 移除会话
    Result<void> removeSession(SessionId id) {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        
        auto it = sessions_.find(id);
        if (it == sessions_.end()) {
            return Result<void>(ErrorCode::SESSION_NOT_FOUND, "Session not found: " + std::to_string(id));
        }
        
        auto session = it->second;
        sessions_.erase(it);
        
        // 触发会话关闭事件
        if (session_handler_) {
            session_handler_->onSessionClosed(session);
        }
        
        return Result<void>();
    }
    
    // 获取会话
    std::shared_ptr<Session> getSession(SessionId id) {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        
        auto it = sessions_.find(id);
        if (it != sessions_.end()) {
            return it->second;
        }
        
        return nullptr;
    }
    
    // 获取所有会话
    std::vector<std::shared_ptr<Session>> getAllSessions() {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        
        std::vector<std::shared_ptr<Session>> result;
        result.reserve(sessions_.size());
        
        for (const auto& pair : sessions_) {
            result.push_back(pair.second);
        }
        
        return result;
    }
    
    // 关闭所有会话
    void closeAllSessions() {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        
        for (const auto& pair : sessions_) {
            pair.second->close();
        }
        
        sessions_.clear();
    }
    
    // 检查空闲会话
    void checkIdleSessions(u64 elapsed_ms) {
        if (config_.idle_timeout_ms == 0) {
            return;
        }
        
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        
        // 遍历所有会话，检查空闲时间
        for (auto it = sessions_.begin(); it != sessions_.end();) {
            auto session = it->second;
            
            // 获取最后活动时间
            u64 last_activity_time = 0;
            if (session->hasAttribute("last_activity_time")) {
                last_activity_time = std::stoull(session->getAttribute("last_activity_time"));
            }
            
            // 计算空闲时间
            u64 now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            u64 idle_time = now - last_activity_time;
            
            // 如果空闲时间超过阈值，触发空闲事件
            if (idle_time >= config_.idle_timeout_ms) {
                if (session_handler_) {
                    session_handler_->onSessionIdle(session);
                }
                
                // 关闭会话
                session->close();
                it = sessions_.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    // 设置会话处理器
    void setSessionHandler(std::unique_ptr<SessionHandler> handler) {
        session_handler_ = std::move(handler);
    }
    
    // 生成新的会话ID
    SessionId generateSessionId() {
        return next_session_id_++;
    }
    
    // 处理接收到的消息
    void handleReceivedMessage(std::shared_ptr<Session> session, std::unique_ptr<Message> message) {
        if (!session || !message) {
            return;
        }
        
        // 更新会话最后活动时间
        session->setAttribute("last_activity_time", std::to_string(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count()));
        
        // 触发消息接收事件
        if (session_handler_) {
            session_handler_->onMessageReceived(session, std::move(message));
        }
    }
    
    // 处理发送的消息
    void handleSentMessage(std::shared_ptr<Session> session, const Message& message) {
        if (!session) {
            return;
        }
        
        // 更新会话最后活动时间
        session->setAttribute("last_activity_time", std::to_string(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count()));
        
        // 触发消息发送事件
        if (session_handler_) {
            session_handler_->onMessageSent(session, message);
        }
    }
    
    // 处理会话错误
    void handleSessionError(std::shared_ptr<Session> session, const Error& error) {
        if (!session) {
            return;
        }
        
        // 触发会话错误事件
        if (session_handler_) {
            session_handler_->onSessionError(session, error);
        }
    }
    
protected:
    NetServiceConfig config_;
    std::unique_ptr<SessionHandler> session_handler_;
    std::unordered_map<SessionId, std::shared_ptr<Session>> sessions_;
    std::mutex sessions_mutex_;
    std::atomic<SessionId> next_session_id_;
};

} // namespace next_gen

#endif // NEXT_GEN_NET_SERVICE_H
