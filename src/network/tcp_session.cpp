#include "../../include/network/tcp_session.h"
#include "../../include/network/tcp_service.h"
#include "../../include/message/message.h"
#include "../../include/network/asio_wrapper.h"
#include <chrono>
#include <mutex>
#include <unordered_map>

namespace next_gen {

// Message header size (category + id + body size)
static constexpr std::size_t HEADER_SIZE = sizeof(MessageCategoryType) + sizeof(MessageIdType) + sizeof(u32);

// Constructor
TcpSession::TcpSession(TcpService* service, asio::io_context& io_context, SessionId id)
    : service_(service),
      socket_(std::make_unique<asio::ip::tcp::socket>(io_context)),
      id_(id),
      state_(SessionState::DISCONNECTED),
      remote_address_(""),
      attributes_mutex_(),
      attributes_() {
    
    // Initialize read buffer
    read_buffer_.resize(HEADER_SIZE);
    
    // Initialize last activity time
    resetIdleTimer();
}

// Destructor
TcpSession::~TcpSession() {
    close();
}

// Get session ID
SessionId TcpSession::getId() const {
    return id_;
}

// Get remote address
std::string TcpSession::getRemoteAddress() const {
    return remote_address_;
}

// Get session state
SessionState TcpSession::getState() const {
    return state_;
}

// Send message
Result<void> TcpSession::send(const Message& message) {
    // Check if session is connected
    if (state_ != SessionState::CONNECTED && state_ != SessionState::AUTHENTICATED) {
        return Result<void>(ErrorCode::CONNECTION_CLOSED, "Session is not connected");
    }
    
    // Serialize message
    auto serialized_result = message.serialize();
    if (serialized_result.has_error()) {
        return Result<void>(serialized_result.error());
    }
    
    auto& serialized_body = serialized_result.value();
    
    // Create buffer with header + body
    std::vector<u8> buffer;
    buffer.resize(HEADER_SIZE + serialized_body.size());
    
    // Write category
    MessageCategoryType category = message.getCategory();
    std::memcpy(buffer.data(), &category, sizeof(category));
    
    // Write ID
    MessageIdType id = message.getId();
    std::memcpy(buffer.data() + sizeof(category), &id, sizeof(id));
    
    // Write body size
    u32 body_size = static_cast<u32>(serialized_body.size());
    std::memcpy(buffer.data() + sizeof(category) + sizeof(id), &body_size, sizeof(body_size));
    
    // Write body
    if (body_size > 0) {
        std::memcpy(buffer.data() + HEADER_SIZE, serialized_body.data(), body_size);
    }
    
    // Lock write queue
    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        
        // Check if write in progress
        bool write_in_progress = !write_queue_.empty();
        
        // Add to write queue
        write_queue_.push(std::move(buffer));
        
        // Start write if not in progress
        if (!write_in_progress) {
            writeMessage();
        }
    }
    
    // Reset idle timer
    resetIdleTimer();
    
    return Result<void>();
}

// Close session
Result<void> TcpSession::close() {
    // Check if already closing or disconnected
    if (state_ == SessionState::CLOSING || state_ == SessionState::DISCONNECTED) {
        return Result<void>();
    }
    
    // Set state to closing
    state_ = SessionState::CLOSING;
    
    // Close socket
    std::error_code ec;
    if (socket_ && socket_->is_open()) {
        socket_->shutdown(asio::ip::tcp::socket::shutdown_both, ec);
        socket_->close(ec);
    }
    
    // Set state to disconnected
    state_ = SessionState::DISCONNECTED;
    
    // Remove session from service
    if (service_) {
        service_->removeSessionById(id_);
    }
    
    return Result<void>();
}

// Get idle time in milliseconds
u64 TcpSession::getIdleTime() const {
    auto now = std::chrono::steady_clock::now();
    auto idle_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_activity_time_).count();
    return static_cast<u64>(idle_time);
}

// Set session attribute
void TcpSession::setAttribute(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(attributes_mutex_);
    attributes_[key] = value;
}

// Get session attribute
std::string TcpSession::getAttribute(const std::string& key) const {
    std::lock_guard<std::mutex> lock(attributes_mutex_);
    auto it = attributes_.find(key);
    if (it != attributes_.end()) {
        return it->second;
    }
    return "";
}

// Check if session attribute exists
bool TcpSession::hasAttribute(const std::string& key) const {
    std::lock_guard<std::mutex> lock(attributes_mutex_);
    return attributes_.find(key) != attributes_.end();
}

// Remove session attribute
void TcpSession::removeAttribute(const std::string& key) {
    std::lock_guard<std::mutex> lock(attributes_mutex_);
    attributes_.erase(key);
}

// Clear all session attributes
void TcpSession::clearAttributes() {
    std::lock_guard<std::mutex> lock(attributes_mutex_);
    attributes_.clear();
}

// Start session (called after accept)
void TcpSession::start() {
    // Get remote address
    try {
        auto endpoint = socket_->remote_endpoint();
        remote_address_ = endpoint.address().to_string() + ":" + 
                         std::to_string(endpoint.port());
    } catch (const std::exception& e) {
        remote_address_ = "unknown";
    }
    
    // Set state to connected
    state_ = SessionState::CONNECTED;
    
    // Start reading
    readHeader();
    
    // Reset idle timer
    resetIdleTimer();
}

// Get socket
asio::ip::tcp::socket& TcpSession::getSocket() {
    return *socket_;
}

// Read header
void TcpSession::readHeader() {
    if (state_ == SessionState::DISCONNECTED || state_ == SessionState::CLOSING) {
        return;
    }
    
    // Read header
    asio::async_read(*socket_,
        asio::buffer(read_buffer_.data(), HEADER_SIZE),
        [this, self = shared_from_this()](const std::error_code& error, std::size_t bytes_transferred) {
            handleReadHeader(error, bytes_transferred);
        });
}

// Read body
void TcpSession::readBody(u32 body_size) {
    if (state_ == SessionState::DISCONNECTED || state_ == SessionState::CLOSING) {
        return;
    }
    
    // Resize buffer to accommodate body
    read_buffer_.resize(HEADER_SIZE + body_size);
    
    // Read body
    asio::async_read(*socket_,
        asio::buffer(read_buffer_.data() + HEADER_SIZE, body_size),
        [this, self = shared_from_this()](const std::error_code& error, std::size_t bytes_transferred) {
            handleReadBody(error, bytes_transferred);
        });
}

// Write message
void TcpSession::writeMessage() {
    if (state_ == SessionState::DISCONNECTED || state_ == SessionState::CLOSING) {
        return;
    }
    
    // Get buffer from queue
    const auto& buffer = write_queue_.front();
    
    // Write buffer
    asio::async_write(*socket_,
        asio::buffer(buffer.data(), buffer.size()),
        [this, self = shared_from_this()](const std::error_code& error, std::size_t bytes_transferred) {
            handleWrite(error, bytes_transferred);
        });
}

// Handle read header
void TcpSession::handleReadHeader(const std::error_code& error, std::size_t bytes_transferred) {
    if (error) {
        // Handle error
        service_->handleSessionErrorById(shared_from_this(), 
            Error(ErrorCode::NETWORK_ERROR, "Read header error: " + error.message()));
        close();
        return;
    }
    
    // Reset idle timer
    resetIdleTimer();
    
    // Extract header fields
    MessageCategoryType category;
    std::memcpy(&category, read_buffer_.data(), sizeof(category));
    
    MessageIdType id;
    std::memcpy(&id, read_buffer_.data() + sizeof(category), sizeof(id));
    
    u32 body_size;
    std::memcpy(&body_size, read_buffer_.data() + sizeof(category) + sizeof(id), sizeof(body_size));
    
    // Check body size
    if (body_size > 0) {
        // Read body
        readBody(body_size);
    } else {
        // Create message
        auto message = DefaultMessageFactory::instance().createMessage(category, id);
        if (!message) {
            // Handle error
            service_->handleSessionErrorById(shared_from_this(),
                Error(ErrorCode::INVALID_MESSAGE, "Invalid message category or ID"));
            
            // Continue reading
            read_buffer_.resize(HEADER_SIZE);
            readHeader();
            return;
        }
        
        // Trigger message received event
        service_->handleReceivedMessageById(shared_from_this(), std::move(message));
        
        // Continue reading
        read_buffer_.resize(HEADER_SIZE);
        readHeader();
    }
}

// Handle read body
void TcpSession::handleReadBody(const std::error_code& error, std::size_t bytes_transferred) {
    if (error) {
        // Handle error
        service_->handleSessionErrorById(shared_from_this(),
            Error(ErrorCode::NETWORK_ERROR, "Read body error: " + error.message()));
        close();
        return;
    }
    
    // Reset idle timer
    resetIdleTimer();
    
    // Extract header fields
    MessageCategoryType category;
    std::memcpy(&category, read_buffer_.data(), sizeof(category));
    
    MessageIdType id;
    std::memcpy(&id, read_buffer_.data() + sizeof(category), sizeof(id));
    
    u32 body_size;
    std::memcpy(&body_size, read_buffer_.data() + sizeof(category) + sizeof(id), sizeof(body_size));
    
    // Create message
    auto message = DefaultMessageFactory::instance().createMessage(category, id);
    if (!message) {
        // Handle error
        service_->handleSessionErrorById(shared_from_this(),
            Error(ErrorCode::INVALID_MESSAGE, "Invalid message category or ID"));
        
        // Continue reading
        read_buffer_.resize(HEADER_SIZE);
        readHeader();
        return;
    }
    
    // Deserialize message
    std::vector<u8> body_data(read_buffer_.begin() + HEADER_SIZE, read_buffer_.end());
    auto result = message->deserialize(body_data);
    if (result.has_error()) {
        // Handle error
        service_->handleSessionErrorById(shared_from_this(),
            Error(ErrorCode::INVALID_MESSAGE, "Failed to deserialize message: " + result.error().message()));
        
        // Continue reading
        read_buffer_.resize(HEADER_SIZE);
        readHeader();
        return;
    }
    
    // Trigger message received event
    service_->handleReceivedMessageById(shared_from_this(), std::move(message));
    
    // Continue reading
    read_buffer_.resize(HEADER_SIZE);
    readHeader();
}

// Handle write
void TcpSession::handleWrite(const std::error_code& error, std::size_t bytes_transferred) {
    if (error) {
        // Handle error
        service_->handleSessionErrorById(shared_from_this(),
            Error(ErrorCode::NETWORK_ERROR, "Write error: " + error.message()));
        close();
        return;
    }
    
    // Reset idle timer
    resetIdleTimer();
    
    // Lock write queue
    std::lock_guard<std::mutex> lock(write_mutex_);
    
    // Remove sent message
    write_queue_.pop();
    
    // Continue writing if more messages
    if (!write_queue_.empty()) {
        writeMessage();
    }
}

// Reset idle timer
void TcpSession::resetIdleTimer() {
    last_activity_time_ = std::chrono::steady_clock::now();
}

} // namespace next_gen
