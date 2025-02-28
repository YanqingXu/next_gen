#include "../../include/network/net_service.h"
#include <chrono>
#include <algorithm>

namespace next_gen {

// Default constructor
NetService::NetService(const std::string& name, const NetServiceConfig& config)
    : BaseService(name),
      config_(config),
      session_handler_(nullptr),
      next_session_id_(1),
      total_connections_(0),
      total_messages_received_(0),
      total_messages_sent_(0),
      total_bytes_received_(0),
      total_bytes_sent_(0),
      last_idle_check_(0) {
}

// Destructor
NetService::~NetService() {
    // Stop the service if it's running
    if (isRunning()) {
        stop();
    }
    
    // Ensure all sessions are closed
    closeAllSessions();
}

// Initialize network service
Result<void> NetService::onInit() {
    NEXT_GEN_LOG_INFO("Initializing network service: " + getName());
    
    // Initialize network library (implementation specific)
    auto result = initNetworkLibrary();
    if (result.has_error()) {
        NEXT_GEN_LOG_ERROR("Failed to initialize network library: " + result.error().message());
        return result;
    }
    
    // Create default session handler if none provided
    if (!session_handler_) {
        session_handler_ = std::make_unique<SessionHandler>();
    }
    
    return Result<void>();
}

// Start network service
Result<void> NetService::onStart() {
    NEXT_GEN_LOG_INFO("Starting network service: " + getName() + 
                     " on " + config_.bind_address + ":" + std::to_string(config_.port));
    
    // Start accepting connections (implementation specific)
    auto result = startServer();
    if (result.has_error()) {
        NEXT_GEN_LOG_ERROR("Failed to start accepting connections: " + result.error().message());
        return result;
    }
    
    // Reset last idle check time
    last_idle_check_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    return Result<void>();
}

// Stop network service
Result<void> NetService::onStop() {
    NEXT_GEN_LOG_INFO("Stopping network service: " + getName());
    
    // Stop accepting connections (implementation specific)
    auto result = stopServer();
    if (result.has_error()) {
        NEXT_GEN_LOG_ERROR("Failed to stop accepting connections: " + result.error().message());
        // Continue anyway to clean up
    }
    
    // Close all active sessions
    closeAllSessions();
    
    // Cleanup network library (implementation specific)
    result = cleanupNetworkLibrary();
    if (result.has_error()) {
        NEXT_GEN_LOG_ERROR("Failed to cleanup network library: " + result.error().message());
        // Continue anyway
    }
    
    return Result<void>();
}

// Update network service
Result<void> NetService::onUpdate(u64 elapsed_ms) {
    // Check for idle sessions
    checkIdleSessions(elapsed_ms);
    
    // Implementation specific update tasks
    auto result = updateNetworkTasks(elapsed_ms);
    if (result.has_error()) {
        NEXT_GEN_LOG_ERROR("Failed to update network tasks: " + result.error().message());
        return result;
    }
    
    return Result<void>();
}

// Add session
Result<void> NetService::addSession(std::shared_ptr<Session> session) {
    if (!session) {
        return Result<void>(ErrorCode::INVALID_ARGUMENT, "Session is null");
    }
    
    SessionId id = session->getId();
    
    // Lock sessions for thread safety
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    // Check if session already exists
    if (sessions_.find(id) != sessions_.end()) {
        return Result<void>(ErrorCode::SESSION_ALREADY_EXISTS, 
                           "Session already exists with ID: " + std::to_string(id));
    }
    
    // Add session to map
    sessions_[id] = session;
    
    // Increment total connections
    total_connections_++;
    
    // Notify session created
    if (session_handler_) {
        session_handler_->onSessionCreated(session);
    }
    
    NEXT_GEN_LOG_INFO("Session created: " + std::to_string(id) + 
                     " from " + session->getRemoteAddress());
    
    return Result<void>();
}

// Remove session
Result<void> NetService::removeSession(SessionId id) {
    // Lock sessions for thread safety
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    // Find session
    auto it = sessions_.find(id);
    if (it == sessions_.end()) {
        return Result<void>(ErrorCode::SESSION_NOT_FOUND, 
                           "Session not found with ID: " + std::to_string(id));
    }
    
    // Get session before removing
    auto session = it->second;
    
    // Remove from map
    sessions_.erase(it);
    
    // Notify session closed
    if (session_handler_) {
        session_handler_->onSessionClosed(session);
    }
    
    NEXT_GEN_LOG_INFO("Session removed: " + std::to_string(id));
    
    return Result<void>();
}

// Get session
std::shared_ptr<Session> NetService::getSession(SessionId id) {
    // Lock sessions for thread safety
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    // Find session
    auto it = sessions_.find(id);
    if (it == sessions_.end()) {
        return nullptr;
    }
    
    return it->second;
}

// Get all sessions
std::vector<std::shared_ptr<Session>> NetService::getAllSessions() {
    std::vector<std::shared_ptr<Session>> result;
    
    // Lock sessions for thread safety
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    // Reserve space for efficiency
    result.reserve(sessions_.size());
    
    // Copy all sessions to vector
    for (const auto& pair : sessions_) {
        result.push_back(pair.second);
    }
    
    return result;
}

// Close all sessions
void NetService::closeAllSessions() {
    std::vector<std::shared_ptr<Session>> sessions;
    
    // Get copy of all sessions to avoid deadlock
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (const auto& pair : sessions_) {
            sessions.push_back(pair.second);
        }
    }
    
    // Close each session
    for (auto& session : sessions) {
        session->close();
    }
}

// Check idle sessions
void NetService::checkIdleSessions(u64 elapsed_ms) {
    // Skip if idle timeout is disabled
    if (config_.idle_timeout_ms == 0) {
        return;
    }
    
    // Add elapsed time to last check time
    last_idle_check_ += elapsed_ms;
    
    // Only check every second to avoid excessive checking
    if (elapsed_ms < 1000) {
        return;
    }
    
    std::vector<std::shared_ptr<Session>> idle_sessions;
    
    // Find idle sessions
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        
        for (const auto& pair : sessions_) {
            auto session = pair.second;
            
            // Skip sessions that are not connected
            if (session->getState() != SessionState::CONNECTED && 
                session->getState() != SessionState::AUTHENTICATED) {
                continue;
            }
            
            // Check if session is idle
            if (session->getIdleTime() > config_.idle_timeout_ms) {
                idle_sessions.push_back(session);
            }
        }
    }
    
    // Handle idle sessions
    for (auto& session : idle_sessions) {
        // Notify idle event
        if (session_handler_) {
            session_handler_->onSessionIdle(session);
        }
        
        NEXT_GEN_LOG_INFO("Session idle timeout: " + std::to_string(session->getId()));
        
        // Close idle session
        session->close();
    }
    
    // Reset last check time
    last_idle_check_ = 0;
}

// Set session handler
void NetService::setSessionHandler(std::unique_ptr<SessionHandler> handler) {
    session_handler_ = std::move(handler);
}

// Generate new session ID
SessionId NetService::generateSessionId() {
    return next_session_id_++;
}

// Handle received message
void NetService::handleReceivedMessage(std::shared_ptr<Session> session, std::unique_ptr<Message> message) {
    if (!session || !message) {
        return;
    }
    
    // Increment message counter
    total_messages_received_++;
    
    // Add message size to byte counter
    auto serialized = message->serialize();
    if (!serialized.has_error()) {
        total_bytes_received_ += serialized.value().size();
    }
    
    // Notify message received
    if (session_handler_) {
        session_handler_->onMessageReceived(session, std::move(message));
    }
}

// Handle sent message
void NetService::handleSentMessage(std::shared_ptr<Session> session, const Message& message) {
    if (!session) {
        return;
    }
    
    // Increment message counter
    total_messages_sent_++;
    
    // Add message size to byte counter
    auto serialized = message.serialize();
    if (!serialized.has_error()) {
        total_bytes_sent_ += serialized.value().size();
    }
    
    // Notify message sent
    if (session_handler_) {
        session_handler_->onMessageSent(session, message);
    }
}

// Handle session error
void NetService::handleSessionError(std::shared_ptr<Session> session, const Error& error) {
    if (!session) {
        return;
    }
    
    NEXT_GEN_LOG_ERROR("Session error: " + std::to_string(session->getId()) + 
                      " - " + error.message());
    
    // Notify session error
    if (session_handler_) {
        session_handler_->onSessionError(session, error);
    }
}

} // namespace next_gen
