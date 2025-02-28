#include "../../include/network/tcp_service.h"
#include "../../include/network/tcp_session.h"
#include "../../include/network/asio_wrapper.h"

namespace next_gen {

// Constructor
TcpService::TcpService(const std::string& name, const TcpServiceConfig& config)
    : NetService(name, config),
      io_context_(nullptr),
      acceptor_(nullptr),
      tcp_config_(config),
      running_(false) {
}

// Destructor
TcpService::~TcpService() {
    // Stop service if running
    if (running_) {
        stopServer();
    }
    
    // Wait for IO threads to finish
    for (auto& thread : io_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

// Initialize network library
Result<void> TcpService::initNetworkLibrary() {
    try {
        // Create IO context
        io_context_ = std::make_unique<asio::io_context>();
        
        return Result<void>();
    } catch (const std::exception& e) {
        return Result<void>(ErrorCode::NETWORK_ERROR, 
                           "Failed to initialize Asio: " + std::string(e.what()));
    }
}

// Cleanup network library
Result<void> TcpService::cleanupNetworkLibrary() {
    // Stop IO context
    if (io_context_) {
        io_context_->stop();
    }
    
    // Wait for IO threads to finish
    for (auto& thread : io_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    // Clear IO threads
    io_threads_.clear();
    
    // Reset IO context and acceptor
    acceptor_.reset();
    io_context_.reset();
    
    return Result<void>();
}

// Start server
Result<void> TcpService::startServer() {
    try {
        // Create endpoint
        asio::ip::tcp::endpoint endpoint(
            asio::ip::address::from_string(tcp_config_.bind_address),
            tcp_config_.port);
        
        // Create acceptor
        acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(*io_context_, endpoint);
        
        // Set acceptor options
        acceptor_->set_option(asio::ip::tcp::acceptor::reuse_address(tcp_config_.reuse_address));
        acceptor_->set_option(asio::socket_base::receive_buffer_size(tcp_config_.socket_recv_buffer_size));
        acceptor_->set_option(asio::socket_base::send_buffer_size(tcp_config_.socket_send_buffer_size));
        
        // Start accepting connections
        acceptConnection();
        
        // Start IO threads
        running_ = true;
        for (u32 i = 0; i < tcp_config_.io_thread_count; ++i) {
            io_threads_.emplace_back([this]() {
                try {
                    io_context_->run();
                } catch (const std::exception& e) {
                    NEXT_GEN_LOG_ERROR("IO thread exception: " + std::string(e.what()));
                }
            });
        }
        
        return Result<void>();
    } catch (const std::exception& e) {
        return Result<void>(ErrorCode::NETWORK_ERROR, 
                           "Failed to start server: " + std::string(e.what()));
    }
}

// Stop server
Result<void> TcpService::stopServer() {
    try {
        // Set running flag to false
        running_ = false;
        
        // Close acceptor
        if (acceptor_ && acceptor_->is_open()) {
            acceptor_->close();
        }
        
        // Stop IO context
        if (io_context_) {
            io_context_->stop();
        }
        
        return Result<void>();
    } catch (const std::exception& e) {
        return Result<void>(ErrorCode::NETWORK_ERROR, 
                           "Failed to stop server: " + std::string(e.what()));
    }
}

// Update network tasks
Result<void> TcpService::updateNetworkTasks(u64 elapsed_ms) {
    // Nothing to do here, Asio handles IO in separate threads
    return Result<void>();
}

// Accept new connection
void TcpService::acceptConnection() {
    if (!running_) {
        return;
    }
    
    if (!acceptor_ || !acceptor_->is_open()) {
        return;
    }
    
    // Create new session
    auto session = std::make_shared<TcpSession>(this, *io_context_, generateSessionId());
    
    // Accept connection
    acceptor_->async_accept(session->getSocket(),
        [this, session](const std::error_code& error) {
            handleAccept(session, error);
        });
}

// Handle new connection
void TcpService::handleAccept(std::shared_ptr<TcpSession> session, const std::error_code& error) {
    if (error) {
        NEXT_GEN_LOG_ERROR("Accept error: " + error.message());
    } else {
        try {
            // Configure socket
            auto& socket = session->getSocket();
            socket.set_option(asio::ip::tcp::no_delay(tcp_config_.tcp_no_delay));
            socket.set_option(asio::socket_base::keep_alive(tcp_config_.keep_alive));
            
            // Start session
            session->start();
            
            // Add session to service
            addSession(session);
        } catch (const std::exception& e) {
            NEXT_GEN_LOG_ERROR("Failed to configure new connection: " + std::string(e.what()));
        }
    }
    
    // Continue accepting connections
    acceptConnection();
}

// Remove a session by ID
Result<void> TcpService::removeSessionById(SessionId id) {
    // Call the parent class method to remove the session
    return removeSession(id);
}

// Handle session error
void TcpService::handleSessionErrorById(std::shared_ptr<Session> session, const Error& error) {
    // Call the parent class method to handle the session error
    handleSessionError(session, error);
}

// Handle received message
void TcpService::handleReceivedMessageById(std::shared_ptr<Session> session, std::unique_ptr<Message> message) {
    // Call the parent class method to handle the received message
    handleReceivedMessage(session, std::move(message));
}

// Handle sent message
void TcpService::handleSentMessageById(std::shared_ptr<Session> session, const Message& message) {
    // Call the parent class method to handle the sent message
    handleSentMessage(session, message);
}

} // namespace next_gen
