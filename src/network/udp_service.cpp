#include "../../include/network/udp_service.h"
#include "../../include/utils/logger.h"
#include "../../include/message/message.h"

namespace next_gen {

// UdpSession implementation
UdpSession::UdpSession(SessionId id, const UdpEndpointId& endpoint_id)
    : id_(id), 
      endpoint_id_(endpoint_id), 
      state_(SessionState::CONNECTED) {
    updateLastActivity();
}

UdpSession::~UdpSession() {
    state_ = SessionState::DISCONNECTED;
}

SessionId UdpSession::getId() const {
    return id_;
}

std::string UdpSession::getRemoteAddress() const {
    return endpoint_id_.address + ":" + std::to_string(endpoint_id_.port);
}

SessionState UdpSession::getState() const {
    return state_;
}

u64 UdpSession::getIdleTime() const {
    std::lock_guard<std::mutex> lock(last_activity_mutex_);
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - last_activity_).count();
}

Result<void> UdpSession::send(const Message& message) {
    // The actual sending is handled by the UdpService
    // This just a placeholder as the UdpService will use sendTo directly
    return Result<void>::success();
}

Result<void> UdpSession::close() {
    if (state_ == SessionState::DISCONNECTED) {
        return Result<void>::error("Session already closed");
    }
    
    state_ = SessionState::DISCONNECTED;
    return Result<void>::success();
}

void UdpSession::setAttribute(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(attributes_mutex_);
    attributes_[key] = value;
}

std::string UdpSession::getAttribute(const std::string& key) const {
    std::lock_guard<std::mutex> lock(attributes_mutex_);
    auto it = attributes_.find(key);
    if (it != attributes_.end()) {
        return it->second;
    }
    return "";
}

bool UdpSession::hasAttribute(const std::string& key) const {
    std::lock_guard<std::mutex> lock(attributes_mutex_);
    return attributes_.find(key) != attributes_.end();
}

void UdpSession::removeAttribute(const std::string& key) {
    std::lock_guard<std::mutex> lock(attributes_mutex_);
    attributes_.erase(key);
}

void UdpSession::clearAttributes() {
    std::lock_guard<std::mutex> lock(attributes_mutex_);
    attributes_.clear();
}

void UdpSession::updateLastActivity() {
    std::lock_guard<std::mutex> lock(last_activity_mutex_);
    last_activity_ = std::chrono::steady_clock::now();
}

const UdpEndpointId& UdpSession::getEndpointId() const {
    return endpoint_id_;
}

// UdpService implementation
UdpService::UdpService(const std::string& name, const UdpServiceConfig& config)
    : NetService(name, config),
      udp_config_(config),
      socket_(nullptr),
      receiving_(false),
      last_cleanup_time_(0) {
    receive_buffer_.resize(config.max_datagram_size);
}

UdpService::~UdpService() {
    // Ensure socket is closed
    if (socket_) {
        asio::error_code ec;
        socket_->close(ec);
        socket_.reset();
    }
}

Result<void> UdpService::initNetworkLibrary() {
    // ASIO doesn't require explicit initialization
    return Result<void>::success();
}

Result<void> UdpService::cleanupNetworkLibrary() {
    // ASIO doesn't require explicit cleanup
    return Result<void>::success();
}

Result<void> UdpService::startServer() {
    try {
        // Create and bind socket
        asio::ip::udp::endpoint endpoint(
            asio::ip::address::from_string(config_.bind_address),
            config_.port
        );
        
        socket_ = std::make_unique<AsioUdpSocket>(io_service_);
        socket_->open(endpoint.protocol());
        
        // Set socket options
        socket_->set_option(asio::socket_base::reuse_address(config_.reuse_address));
        
        socket_->bind(endpoint);
        
        // Start receiving
        receiving_ = true;
        startReceive();
        
        Logger::info("{}: UDP server started on {}:{}", 
            getName(), config_.bind_address, config_.port);
        
        return Result<void>::success();
    } catch (const std::exception& e) {
        return Result<void>::error("Failed to start UDP server: {}", e.what());
    }
}

Result<void> UdpService::stopServer() {
    receiving_ = false;
    
    if (socket_) {
        try {
            asio::error_code ec;
            socket_->close(ec);
            socket_.reset();
            
            Logger::info("{}: UDP server stopped", getName());
            
            return Result<void>::success();
        } catch (const std::exception& e) {
            return Result<void>::error("Failed to stop UDP server: {}", e.what());
        }
    }
    
    return Result<void>::success();
}

Result<void> UdpService::updateNetworkTasks(u64 elapsed_ms) {
    try {
        // Poll IO service
        io_service_.poll();
        
        // Clean up inactive sessions
        cleanupInactiveSessions(elapsed_ms);
        
        return Result<void>::success();
    } catch (const std::exception& e) {
        return Result<void>::error("Error updating UDP service: {}", e.what());
    }
}

Result<void> UdpService::sendTo(const UdpEndpointId& endpoint_id, const void* data, size_t size) {
    if (!socket_ || !socket_->is_open()) {
        return Result<void>::error("Socket not open");
    }
    
    try {
        // Create endpoint
        asio::ip::udp::endpoint endpoint(
            asio::ip::address::from_string(endpoint_id.address),
            endpoint_id.port
        );
        
        // Send data
        socket_->send_to(
            asio::buffer(data, size),
            endpoint
        );
        
        // Update statistics
        total_bytes_sent_ += size;
        total_messages_sent_++;
        
        return Result<void>::success();
    } catch (const std::exception& e) {
        return Result<void>::error("Failed to send datagram: {}", e.what());
    }
}

void UdpService::startReceive() {
    if (!receiving_ || !socket_ || !socket_->is_open()) {
        return;
    }
    
    socket_->async_receive_from(
        asio::buffer(receive_buffer_),
        remote_endpoint_,
        [this](const asio::error_code& error, std::size_t bytes_transferred) {
            this->handleReceive(error, bytes_transferred);
        }
    );
}

void UdpService::handleReceive(const asio::error_code& error, std::size_t bytes_transferred) {
    if (!receiving_) {
        return;
    }
    
    if (!error && bytes_transferred > 0) {
        // Get the remote endpoint details
        UdpEndpointId endpoint_id{
            remote_endpoint_.address().to_string(),
            remote_endpoint_.port()
        };
        
        // Update statistics
        total_bytes_received_ += bytes_transferred;
        total_messages_received_++;
        
        // Process the received datagram
        handleDatagram(endpoint_id, receive_buffer_.data(), bytes_transferred);
    } else if (error) {
        Logger::error("{}: Error receiving datagram: {}", getName(), error.message());
    }
    
    // Continue receiving
    startReceive();
}

void UdpService::handleDatagram(const UdpEndpointId& endpoint_id, const void* data, size_t size) {
    // Get or create session for this endpoint
    auto session = getOrCreateSession(endpoint_id);
    if (!session) {
        Logger::error("{}: Failed to create session for endpoint {}:{}", 
            getName(), endpoint_id.address, endpoint_id.port);
        return;
    }
    
    // Update session activity time
    session->updateLastActivity();
    
    // Here you would typically parse the datagram into a Message object
    // and then call handleReceivedMessage
    
    // As a placeholder, we'll just log the reception
    Logger::debug("{}: Received {} bytes from {}:{}", 
        getName(), size, endpoint_id.address, endpoint_id.port);
    
    // Subclasses should override this method to implement custom datagram handling
}

std::shared_ptr<UdpSession> UdpService::getOrCreateSession(const UdpEndpointId& endpoint_id) {
    std::lock_guard<std::mutex> lock(endpoint_map_mutex_);
    
    // Check if we already have a session for this endpoint
    auto it = endpoint_to_session_.find(endpoint_id);
    if (it != endpoint_to_session_.end()) {
        // We have a session ID, get the session
        auto session = std::dynamic_pointer_cast<UdpSession>(getSession(it->second));
        if (session) {
            return session;
        }
        // Session not found, will create a new one
    }
    
    // Create a new session
    SessionId sessionId = generateSessionId();
    auto session = std::make_shared<UdpSession>(sessionId, endpoint_id);
    
    // Add the session
    if (auto result = addSession(session); !result) {
        Logger::error("{}: Failed to add session: {}", 
            getName(), result.getError().getMessage());
        return nullptr;
    }
    
    // Map the endpoint to the session ID
    endpoint_to_session_[endpoint_id] = sessionId;
    
    // Notify session creation
    if (session_handler_) {
        session_handler_->onSessionCreated(session);
        session_handler_->onSessionOpened(session);
    }
    
    Logger::debug("{}: Created new session {} for endpoint {}:{}", 
        getName(), sessionId, endpoint_id.address, endpoint_id.port);
    
    return session;
}

void UdpService::cleanupInactiveSessions(u64 elapsed_ms) {
    std::lock_guard<std::mutex> lock(inactive_session_mutex_);
    
    // Calculate time since last cleanup
    u64 now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
    
    // Only cleanup every few seconds to avoid doing this too often
    if (last_cleanup_time_ == 0) {
        last_cleanup_time_ = now;
        return;
    }
    
    if (now - last_cleanup_time_ < 5000) { // Every 5 seconds
        return;
    }
    
    last_cleanup_time_ = now;
    
    // Get all sessions
    auto sessions = getAllSessions();
    std::vector<SessionId> to_remove;
    
    // Find inactive sessions
    for (auto& session : sessions) {
        auto udp_session = std::dynamic_pointer_cast<UdpSession>(session);
        if (!udp_session) {
            continue;
        }
        
        u64 idle_time = udp_session->getIdleTime();
        if (idle_time > udp_config_.session_timeout_ms) {
            to_remove.push_back(session->getId());
            
            // Remove from endpoint map
            std::lock_guard<std::mutex> map_lock(endpoint_map_mutex_);
            auto& endpoint_id = udp_session->getEndpointId();
            endpoint_to_session_.erase(endpoint_id);
            
            Logger::debug("{}: Removing inactive session {} (idle for {} ms)", 
                getName(), session->getId(), idle_time);
            
            // Notify session closure
            if (session_handler_) {
                session_handler_->onSessionClosed(session);
            }
        }
    }
    
    // Remove inactive sessions
    for (auto id : to_remove) {
        removeSession(id);
    }
}

} // namespace next_gen
