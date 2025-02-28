#ifndef NEXT_GEN_TCP_SERVICE_H
#define NEXT_GEN_TCP_SERVICE_H

#include "net_service.h"
#include "asio_wrapper.h"
#include <atomic>
#include <memory>

namespace next_gen {

// TCP session forward declaration
class TcpSession;

// TCP service configuration
struct TcpServiceConfig : public NetServiceConfig {
    // Number of IO threads
    u32 io_thread_count = 1;
    
    // Accept backlog
    u32 accept_backlog = 128;
    
    // Socket send buffer size
    u32 socket_send_buffer_size = 8192;
    
    // Socket receive buffer size
    u32 socket_recv_buffer_size = 8192;
};

// TCP network service implementation
class NEXT_GEN_API TcpService : public NetService {
public:
    // Constructor
    TcpService(const std::string& name, const TcpServiceConfig& config = TcpServiceConfig());
    
    // Destructor
    ~TcpService() override;
    
    // Remove a session by ID
    Result<void> removeSessionById(SessionId id);
    
    // Handle session error
    void handleSessionErrorById(std::shared_ptr<Session> session, const Error& error);
    
    // Handle received message
    void handleReceivedMessageById(std::shared_ptr<Session> session, std::unique_ptr<Message> message);
    
    // Handle sent message
    void handleSentMessageById(std::shared_ptr<Session> session, const Message& message);
    
protected:
    // Initialize network library
    Result<void> initNetworkLibrary() override;
    
    // Cleanup network library
    Result<void> cleanupNetworkLibrary() override;
    
    // Start server
    Result<void> startServer() override;
    
    // Stop server
    Result<void> stopServer() override;
    
    // Update network tasks
    Result<void> updateNetworkTasks(u64 elapsed_ms) override;
    
private:
    // Handle new connection
    void handleAccept(std::shared_ptr<TcpSession> session, const std::error_code& error);
    
    // Accept new connection
    void acceptConnection();
    
    // IO context for Asio
    std::unique_ptr<asio::io_context> io_context_;
    
    // Acceptor for incoming connections
    std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
    
    // IO threads
    std::vector<std::thread> io_threads_;
    
    // TCP-specific configuration
    TcpServiceConfig tcp_config_;
    
    // Running flag
    std::atomic<bool> running_;
};

} // namespace next_gen

#endif // NEXT_GEN_TCP_SERVICE_H
