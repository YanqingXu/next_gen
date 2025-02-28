#ifndef NEXT_GEN_MESSAGE_H
#define NEXT_GEN_MESSAGE_H

#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <type_traits>
#include <unordered_map>
#include "../core/config.h"
#include "../utils/error.h"

namespace next_gen {

// Message ID types
using MessageCategoryType = u8;
using MessageIdType = u16;

// Base message class
class NEXT_GEN_API Message {
public:
    Message(MessageCategoryType category, MessageIdType id)
        : category_(category), id_(id), session_id_(0), timestamp_(0) {}
    
    virtual ~Message() = default;
    
    // Get message category
    MessageCategoryType getCategory() const { return category_; }
    
    // Get message ID
    MessageIdType getId() const { return id_; }
    
    // Get session ID
    u32 getSessionId() const { return session_id_; }
    
    // Set session ID
    void setSessionId(u32 session_id) { session_id_ = session_id; }
    
    // Get timestamp
    u64 getTimestamp() const { return timestamp_; }
    
    // Set timestamp
    void setTimestamp(u64 timestamp) { timestamp_ = timestamp; }
    
    // Get message name
    virtual std::string getName() const { return "Message"; }
    
    // Serialize message
    virtual Result<std::vector<u8>> serialize() const {
        return Result<std::vector<u8>>(ErrorCode::NOT_IMPLEMENTED, "Serialization not implemented");
    }
    
    // Deserialize message
    virtual Result<void> deserialize(const std::vector<u8>& data) {
        return Result<void>(ErrorCode::NOT_IMPLEMENTED, "Deserialization not implemented");
    }
    
    // Clone message
    virtual std::unique_ptr<Message> clone() const {
        return std::make_unique<Message>(category_, id_);
    }
    
    // Convert to string
    virtual std::string toString() const {
        return "Message[category=" + std::to_string(category_) + 
               ", id=" + std::to_string(id_) + 
               ", session_id=" + std::to_string(session_id_) + 
               ", timestamp=" + std::to_string(timestamp_) + "]";
    }
    
protected:
    MessageCategoryType category_;
    MessageIdType id_;
    u32 session_id_;
    u64 timestamp_;
};

// Message factory interface
class NEXT_GEN_API MessageFactory {
public:
    virtual ~MessageFactory() = default;
    
    // Create message
    virtual std::unique_ptr<Message> createMessage(MessageCategoryType category, MessageIdType id) = 0;
    
    // Register message type
    template<typename T>
    void registerMessageType() {
        static_assert(std::is_base_of<Message, T>::value, "T must be derived from Message");
        registerMessageCreator(T::CATEGORY, T::ID, []() -> std::unique_ptr<Message> {
            return std::make_unique<T>();
        });
    }
    
protected:
    // Register message creator
    virtual void registerMessageCreator(MessageCategoryType category, MessageIdType id, 
                                       std::function<std::unique_ptr<Message>()> creator) = 0;
};

// Default message factory implementation
class NEXT_GEN_API DefaultMessageFactory : public MessageFactory {
public:
    static DefaultMessageFactory& instance() {
        static DefaultMessageFactory instance;
        return instance;
    }
    
    // Create message
    std::unique_ptr<Message> createMessage(MessageCategoryType category, MessageIdType id) override {
        auto key = makeKey(category, id);
        auto it = creators_.find(key);
        if (it != creators_.end()) {
            return it->second();
        }
        return std::make_unique<Message>(category, id);
    }
    
protected:
    // Register message creator
    void registerMessageCreator(MessageCategoryType category, MessageIdType id, 
                               std::function<std::unique_ptr<Message>()> creator) override {
        auto key = makeKey(category, id);
        creators_[key] = creator;
    }
    
private:
    DefaultMessageFactory() = default;
    ~DefaultMessageFactory() = default;
    
    DefaultMessageFactory(const DefaultMessageFactory&) = delete;
    DefaultMessageFactory& operator=(const DefaultMessageFactory&) = delete;
    
    // Create key
    static u32 makeKey(MessageCategoryType category, MessageIdType id) {
        return (static_cast<u32>(category) << 16) | static_cast<u32>(id);
    }
    
    std::unordered_map<u32, std::function<std::unique_ptr<Message>()>> creators_;
};

// Message handler interface
class NEXT_GEN_API MessageHandler {
public:
    virtual ~MessageHandler() = default;
    
    // Handle message
    virtual void handleMessage(const Message& message) = 0;
};

// Message handler template
template<typename T, typename Handler>
class MessageHandlerImpl : public MessageHandler {
public:
    MessageHandlerImpl(Handler handler) : handler_(handler) {}
    
    void handleMessage(const Message& message) override {
        // Try to convert message to target type
        const T* typedMessage = dynamic_cast<const T*>(&message);
        if (typedMessage) {
            handler_(*typedMessage);
        }
    }
    
private:
    Handler handler_;
};

// Create message handler
template<typename T, typename Handler>
std::unique_ptr<MessageHandler> createMessageHandler(Handler&& handler) {
    static_assert(std::is_base_of<Message, T>::value, "T must be derived from Message");
    return std::make_unique<MessageHandlerImpl<T, Handler>>(std::forward<Handler>(handler));
}

} // namespace next_gen

#endif // NEXT_GEN_MESSAGE_H
