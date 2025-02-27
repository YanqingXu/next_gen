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

// 消息ID类型
using MessageCategoryType = u8;
using MessageIdType = u16;

// 消息基类
class NEXT_GEN_API Message {
public:
    Message(MessageCategoryType category, MessageIdType id)
        : category_(category), id_(id), session_id_(0), timestamp_(0) {}
    
    virtual ~Message() = default;
    
    // 获取消息分类
    MessageCategoryType getCategory() const { return category_; }
    
    // 获取消息ID
    MessageIdType getId() const { return id_; }
    
    // 获取会话ID
    u32 getSessionId() const { return session_id_; }
    
    // 设置会话ID
    void setSessionId(u32 session_id) { session_id_ = session_id; }
    
    // 获取时间戳
    u64 getTimestamp() const { return timestamp_; }
    
    // 设置时间戳
    void setTimestamp(u64 timestamp) { timestamp_ = timestamp; }
    
    // 获取消息名称
    virtual std::string getName() const { return "Message"; }
    
    // 序列化消息
    virtual Result<std::vector<u8>> serialize() const {
        return Result<std::vector<u8>>(ErrorCode::NOT_IMPLEMENTED, "Serialization not implemented");
    }
    
    // 反序列化消息
    virtual Result<void> deserialize(const std::vector<u8>& data) {
        return Result<void>(ErrorCode::NOT_IMPLEMENTED, "Deserialization not implemented");
    }
    
    // 克隆消息
    virtual std::unique_ptr<Message> clone() const {
        return std::make_unique<Message>(category_, id_);
    }
    
    // 转换为字符串
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

// 消息工厂接口
class NEXT_GEN_API MessageFactory {
public:
    virtual ~MessageFactory() = default;
    
    // 创建消息
    virtual std::unique_ptr<Message> createMessage(MessageCategoryType category, MessageIdType id) = 0;
    
    // 注册消息类型
    template<typename T>
    void registerMessageType() {
        static_assert(std::is_base_of<Message, T>::value, "T must be derived from Message");
        registerMessageCreator(T::CATEGORY, T::ID, []() -> std::unique_ptr<Message> {
            return std::make_unique<T>();
        });
    }
    
protected:
    // 注册消息创建器
    virtual void registerMessageCreator(MessageCategoryType category, MessageIdType id, 
                                       std::function<std::unique_ptr<Message>()> creator) = 0;
};

// 默认消息工厂实现
class NEXT_GEN_API DefaultMessageFactory : public MessageFactory {
public:
    static DefaultMessageFactory& instance() {
        static DefaultMessageFactory instance;
        return instance;
    }
    
    // 创建消息
    std::unique_ptr<Message> createMessage(MessageCategoryType category, MessageIdType id) override {
        auto key = makeKey(category, id);
        auto it = creators_.find(key);
        if (it != creators_.end()) {
            return it->second();
        }
        return std::make_unique<Message>(category, id);
    }
    
protected:
    // 注册消息创建器
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
    
    // 创建键
    static u32 makeKey(MessageCategoryType category, MessageIdType id) {
        return (static_cast<u32>(category) << 16) | static_cast<u32>(id);
    }
    
    std::unordered_map<u32, std::function<std::unique_ptr<Message>()>> creators_;
};

// 消息处理器接口
class NEXT_GEN_API MessageHandler {
public:
    virtual ~MessageHandler() = default;
    
    // 处理消息
    virtual void handleMessage(const Message& message) = 0;
};

// 消息处理器模板
template<typename T, typename Handler>
class MessageHandlerImpl : public MessageHandler {
public:
    MessageHandlerImpl(Handler handler) : handler_(handler) {}
    
    void handleMessage(const Message& message) override {
        // 尝试将消息转换为目标类型
        const T* typedMessage = dynamic_cast<const T*>(&message);
        if (typedMessage) {
            handler_(*typedMessage);
        }
    }
    
private:
    Handler handler_;
};

// 创建消息处理器
template<typename T, typename Handler>
std::unique_ptr<MessageHandler> createMessageHandler(Handler&& handler) {
    static_assert(std::is_base_of<Message, T>::value, "T must be derived from Message");
    return std::make_unique<MessageHandlerImpl<T, Handler>>(std::forward<Handler>(handler));
}

} // namespace next_gen

#endif // NEXT_GEN_MESSAGE_H
