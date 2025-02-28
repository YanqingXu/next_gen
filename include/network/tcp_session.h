#ifndef NEXT_GEN_TCP_SESSION_H
#define NEXT_GEN_TCP_SESSION_H

#include "net_service.h"
#include "asio_wrapper.h"
#include <memory>
#include <queue>
#include <mutex>
#include <atomic>
#include <chrono>
#include <unordered_map>

namespace next_gen {

// Forward declaration
class TcpService;

// TCP session implementation
class NEXT_GEN_API TcpSession : public Session, public std::enable_shared_from_this<TcpSession> {
public:
    // Constructor
    TcpSession(TcpService* service, asio::io_context& io_context, SessionId id);
    
    // Destructor
    ~TcpSession() override;
    
    // Get session ID
    SessionId getId() const override;
    
    // Get remote address
    std::string getRemoteAddress() const override;
    
    // Get session state
    SessionState getState() const override;
    
    // Send message
    Result<void> send(const Message& message) override;
    
    // Close session
    Result<void> close() override;
    
    // Get idle time in milliseconds
    u64 getIdleTime() const override;
    
    // Set session attribute
    void setAttribute(const std::string& key, const std::string& value) override;
    
    // Get session attribute
    std::string getAttribute(const std::string& key) const override;
    
    // Check if session attribute exists
    bool hasAttribute(const std::string& key) const override;
    
    // Remove session attribute
    void removeAttribute(const std::string& key) override;
    
    // Clear all session attributes
    void clearAttributes() override;
    
    // Start session (called after accept)
    void start();
    
    // Get socket
    asio::ip::tcp::socket& getSocket();
    
private:
    // Read header
    void readHeader();
    
    // Read body
    void readBody(u32 body_size);
    
    // Write message
    void writeMessage();
    
    // Handle read header
    void handleReadHeader(const std::error_code& error, std::size_t bytes_transferred);
    
    // Handle read body
    void handleReadBody(const std::error_code& error, std::size_t bytes_transferred);
    
    // Handle write
    void handleWrite(const std::error_code& error, std::size_t bytes_transferred);
    
    // Reset idle timer
    void resetIdleTimer();
    
    // Parent service
    TcpService* service_;
    
    // Socket
    std::unique_ptr<asio::ip::tcp::socket> socket_;
    
    // Session ID
    SessionId id_;
    
    // Session state
    std::atomic<SessionState> state_;
    
    // Remote address
    std::string remote_address_;
    
    // Read buffer
    std::vector<u8> read_buffer_;
    
    // Write queue
    std::queue<std::vector<u8>> write_queue_;
    
    // Write mutex
    std::mutex write_mutex_;
    
    // Last activity time
    std::chrono::steady_clock::time_point last_activity_time_;
    
    // Session attributes
    std::unordered_map<std::string, std::string> attributes_;
    
    // Attributes mutex
    mutable std::mutex attributes_mutex_;
};

} // namespace next_gen

#endif // NEXT_GEN_TCP_SESSION_H
