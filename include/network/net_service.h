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

// Session ID type
using SessionId = u32;

// Session state
enum class SessionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    AUTHENTICATING,
    AUTHENTICATED,
    CLOSING
};

// Session interface
class NEXT_GEN_API Session {
public:
    virtual ~Session() = default;
    
    // Get session ID
    virtual SessionId getId() const = 0;
    
    // Get remote address
    virtual std::string getRemoteAddress() const = 0;
    
    // Get session state
    virtual SessionState getState() const = 0;
    
    // Get idle time in milliseconds
    virtual u64 getIdleTime() const = 0;
    
    // Send message
    virtual Result<void> send(const Message& message) = 0;
    
    // Close session
    virtual Result<void> close() = 0;
    
    // Set session attribute
    virtual void setAttribute(const std::string& key, const std::string& value) = 0;
    
    // Get session attribute
    virtual std::string getAttribute(const std::string& key) const = 0;
    
    // Check if session attribute exists
    virtual bool hasAttribute(const std::string& key) const = 0;
    
    // Remove session attribute
    virtual void removeAttribute(const std::string& key) = 0;
    
    // Clear all session attributes
    virtual void clearAttributes() = 0;
};

// Session event handler
class NEXT_GEN_API SessionHandler {
public:
    virtual ~SessionHandler() = default;
    
    // Session created event
    virtual void onSessionCreated(std::shared_ptr<Session> session) {}
    
    // Session opened event
    virtual void onSessionOpened(std::shared_ptr<Session> session) {}
    
    // Session closed event
    virtual void onSessionClosed(std::shared_ptr<Session> session) {}
    
    // Session idle event
    virtual void onSessionIdle(std::shared_ptr<Session> session) {}
    
    // Session error event
    virtual void onSessionError(std::shared_ptr<Session> session, const Error& error) {}
    
    // Message received event
    virtual void onMessageReceived(std::shared_ptr<Session> session, std::unique_ptr<Message> message) {}
    
    // Message sent event
    virtual void onMessageSent(std::shared_ptr<Session> session, const Message& message) {}
};

// Network service configuration
struct NetServiceConfig {
    std::string bind_address = "0.0.0.0";     // Bind address
    u16 port = 0;                             // Port
    u32 max_connections = 1000;               // Maximum connections
    u32 read_buffer_size = 8192;              // Read buffer size
    u32 write_buffer_size = 8192;             // Write buffer size
    u32 idle_timeout_ms = 60000;              // Idle timeout in milliseconds
    bool reuse_address = true;                // Reuse address option
    bool tcp_no_delay = true;                 // TCP no delay option
    bool keep_alive = true;                   // Keep alive option
};

// Network service interface
class NEXT_GEN_API NetService : public BaseService {
public:
    NetService(const std::string& name, const NetServiceConfig& config = NetServiceConfig());
    
    virtual ~NetService();
    
protected:
    // Initialize network service
    Result<void> onInit() override;
    
    // Start network service
    Result<void> onStart() override;
    
    // Stop network service
    Result<void> onStop() override;
    
    // Update network service
    Result<void> onUpdate(u64 elapsed_ms) override;
    
    // Initialize network library (implemented by subclass)
    virtual Result<void> initNetworkLibrary() = 0;
    
    // Cleanup network library (implemented by subclass)
    virtual Result<void> cleanupNetworkLibrary() = 0;
    
    // Start server (implemented by subclass)
    virtual Result<void> startServer() = 0;
    
    // Stop server (implemented by subclass)
    virtual Result<void> stopServer() = 0;
    
    // Update network tasks (implemented by subclass)
    virtual Result<void> updateNetworkTasks(u64 elapsed_ms) = 0;
    
    // Add session
    Result<void> addSession(std::shared_ptr<Session> session);
    
    // Remove session
    Result<void> removeSession(SessionId id);
    
    // Get session
    std::shared_ptr<Session> getSession(SessionId id);
    
    // Get all sessions
    std::vector<std::shared_ptr<Session>> getAllSessions();
    
    // Close all sessions
    void closeAllSessions();
    
    // Check idle sessions
    void checkIdleSessions(u64 elapsed_ms);
    
    // Set session handler
    void setSessionHandler(std::unique_ptr<SessionHandler> handler);
    
    // Generate new session ID
    SessionId generateSessionId();
    
    // Handle received message
    void handleReceivedMessage(std::shared_ptr<Session> session, std::unique_ptr<Message> message);
    
    // Handle sent message
    void handleSentMessage(std::shared_ptr<Session> session, const Message& message);
    
    // Handle session error
    void handleSessionError(std::shared_ptr<Session> session, const Error& error);
    
protected:
    NetServiceConfig config_;
    std::unique_ptr<SessionHandler> session_handler_;
    std::unordered_map<SessionId, std::shared_ptr<Session>> sessions_;
    std::mutex sessions_mutex_;
    std::atomic<SessionId> next_session_id_;
    
    // Statistics
    std::atomic<u64> total_connections_;
    std::atomic<u64> total_messages_received_;
    std::atomic<u64> total_messages_sent_;
    std::atomic<u64> total_bytes_received_;
    std::atomic<u64> total_bytes_sent_;
    std::atomic<u64> last_idle_check_;
};

} // namespace next_gen

#endif // NEXT_GEN_NET_SERVICE_H
