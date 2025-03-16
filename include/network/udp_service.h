#ifndef NEXT_GEN_UDP_SERVICE_H
#define NEXT_GEN_UDP_SERVICE_H

#include <unordered_map>
#include <string>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include "net_service.h"
#include "asio_wrapper.h"

namespace next_gen {

// UDP endpoint identifier (combination of address and port)
struct UdpEndpointId {
    std::string address;
    u16 port;
    
    bool operator==(const UdpEndpointId& other) const {
        return address == other.address && port == other.port;
    }
};

// Custom hash function for UdpEndpointId
struct UdpEndpointIdHash {
    std::size_t operator()(const UdpEndpointId& id) const {
        return std::hash<std::string>()(id.address) ^ std::hash<u16>()(id.port);
    }
};

// UDP session class
class NEXT_GEN_API UdpSession : public Session {
public:
    UdpSession(SessionId id, const UdpEndpointId& endpoint_id);
    virtual ~UdpSession();
    
    // Session interface implementation
    SessionId getId() const override;
    std::string getRemoteAddress() const override;
    SessionState getState() const override;
    u64 getIdleTime() const override;
    Result<void> send(const Message& message) override;
    Result<void> close() override;
    void setAttribute(const std::string& key, const std::string& value) override;
    std::string getAttribute(const std::string& key) const override;
    bool hasAttribute(const std::string& key) const override;
    void removeAttribute(const std::string& key) override;
    void clearAttributes() override;
    
    // UDP-specific methods
    void updateLastActivity();
    const UdpEndpointId& getEndpointId() const;
    
private:
    SessionId id_;
    UdpEndpointId endpoint_id_;
    SessionState state_;
    std::unordered_map<std::string, std::string> attributes_;
    std::mutex attributes_mutex_;
    std::chrono::steady_clock::time_point last_activity_;
    std::mutex last_activity_mutex_;
};

// UDP service configuration
struct UdpServiceConfig : public NetServiceConfig {
    u32 max_datagram_size = 4096;      // Maximum datagram size
    u32 session_timeout_ms = 60000;    // Session timeout in milliseconds
};

// UDP service class
class NEXT_GEN_API UdpService : public NetService {
public:
    UdpService(const std::string& name, const UdpServiceConfig& config = UdpServiceConfig());
    virtual ~UdpService();
    
protected:
    // Network library initialization
    Result<void> initNetworkLibrary() override;
    Result<void> cleanupNetworkLibrary() override;
    
    // Server start/stop
    Result<void> startServer() override;
    Result<void> stopServer() override;
    
    // Network tasks update
    Result<void> updateNetworkTasks(u64 elapsed_ms) override;
    
    // Send datagram to remote endpoint
    Result<void> sendTo(const UdpEndpointId& endpoint_id, const void* data, size_t size);
    
    // Handle received datagram
    virtual void handleDatagram(const UdpEndpointId& endpoint_id, const void* data, size_t size);
    
    // Get or create session for endpoint
    std::shared_ptr<UdpSession> getOrCreateSession(const UdpEndpointId& endpoint_id);
    
private:
    // Start receiving datagrams
    void startReceive();
    
    // Datagram receive handler
    void handleReceive(const asio::error_code& error, std::size_t bytes_transferred);
    
    // Clean up inactive sessions
    void cleanupInactiveSessions(u64 elapsed_ms);
    
private:
    UdpServiceConfig udp_config_;
    AsioService io_service_;
    std::unique_ptr<AsioUdpSocket> socket_;
    AsioUdpEndpoint remote_endpoint_;
    std::vector<u8> receive_buffer_;
    std::atomic<bool> receiving_;
    std::mutex inactive_session_mutex_;
    u64 last_cleanup_time_;
    
    // Map from endpoint ID to session ID
    std::unordered_map<UdpEndpointId, SessionId, UdpEndpointIdHash> endpoint_to_session_;
    std::mutex endpoint_map_mutex_;
};

} // namespace next_gen

#endif // NEXT_GEN_UDP_SERVICE_H
