#pragma once

#include <memory>
#include <string>
#include <vector>
#include "types.h"
#include "../../include/utils/byte_stream.h"

namespace next_gen {
namespace message {

/**
 * @brief 应用层消息基类
 * 
 * 所有应用层消息都继承自这个基类，提供序列化、反序列化和元数据功能
 */
class MessageBase {
public:
    MessageBase(MessageCategoryType category, MessageIdType id)
        : category_(category), id_(id), session_id_(0), timestamp_(0) {}
    
    virtual ~MessageBase() = default;
    
    // 获取消息类别
    MessageCategoryType getCategory() const { return category_; }
    
    // 获取消息ID
    MessageIdType getId() const { return id_; }
    
    // 获取会话ID
    uint32_t getSessionId() const { return session_id_; }
    
    // 设置会话ID
    void setSessionId(uint32_t session_id) { session_id_ = session_id; }
    
    // 获取时间戳
    uint64_t getTimestamp() const { return timestamp_; }
    
    // 设置时间戳
    void setTimestamp(uint64_t timestamp) { timestamp_ = timestamp; }
    
    // 获取消息名称（子类可重写）
    virtual std::string getName() const { return "Unknown"; }
    
    // 获取消息描述（子类可重写）
    virtual std::string getDescription() const { return ""; }
    
    // 获取消息版本（子类可重写）
    virtual uint16_t getVersion() const { return 1; }
    
    // 获取字段信息（子类必须重写）
    virtual std::vector<FieldInfo> getFieldInfo() const = 0;
    
    // 获取消息元数据
    MessageInfo getMessageInfo() const {
        MessageInfo info(category_, id_, getName(), getDescription(), getVersion());
        for (const auto& field : getFieldInfo()) {
            info.addField(field);
        }
        return info;
    }
    
    // 计算序列化后的大小（子类必须重写）
    virtual MessageSizeType getSerializedSize() const = 0;
    
    // 序列化消息（子类必须重写）
    virtual void serialize(ByteStream& stream) const = 0;
    
    // 反序列化消息（子类必须重写）
    virtual void deserialize(ByteStream& stream) = 0;
    
    // 克隆消息（子类必须重写）
    virtual std::unique_ptr<MessageBase> clone() const = 0;
    
    // 获取字符串表示
    virtual std::string toString() const {
        return "Message[category=" + std::to_string(category_) + 
               ", id=" + std::to_string(id_) + 
               ", name=" + getName() + 
               ", version=" + std::to_string(getVersion()) + "]";
    }
    
    // 辅助序列化方法
    std::vector<uint8_t> toBytes() const {
        ByteStream stream;
        // 先写入消息头（分类和ID）
        stream.write(category_);
        stream.write(id_);
        
        // 序列化消息体
        serialize(stream);
        
        return stream.getData();
    }
    
    // 辅助反序列化方法
    bool fromBytes(const std::vector<uint8_t>& data) {
        if (data.size() < sizeof(MessageCategoryType) + sizeof(MessageIdType)) {
            return false;
        }
        
        ByteStream stream(data);
        
        // 读取并校验消息头（分类和ID）
        MessageCategoryType category;
        MessageIdType id;
        stream.read(category);
        stream.read(id);
        
        if (category != category_ || id != id_) {
            return false;
        }
        
        // 反序列化消息体
        deserialize(stream);
        return !stream.hasError();
    }
    
protected:
    MessageCategoryType category_;
    MessageIdType id_;
    uint32_t session_id_;
    uint64_t timestamp_;
};

/**
 * @brief 消息工厂接口
 * 
 * 用于创建和管理消息实例
 */
class MessageFactory {
public:
    virtual ~MessageFactory() = default;
    
    // 创建消息
    virtual std::unique_ptr<MessageBase> createMessage(MessageCategoryType category, MessageIdType id) = 0;
    
    // 注册消息类型
    template<typename T>
    void registerMessageType() {
        static_assert(std::is_base_of<MessageBase, T>::value, "T must be derived from MessageBase");
        registerMessageCreator(T::CATEGORY, T::ID, []() -> std::unique_ptr<MessageBase> {
            return std::make_unique<T>();
        });
    }
    
protected:
    // 注册消息创建器
    virtual void registerMessageCreator(
        MessageCategoryType category, 
        MessageIdType id, 
        std::function<std::unique_ptr<MessageBase>()> creator) = 0;
};

/**
 * @brief 消息处理器接口
 * 
 * 用于处理消息的基类
 */
class MessageHandler {
public:
    virtual ~MessageHandler() = default;
    
    // 处理消息
    virtual bool handleMessage(const MessageBase& message) = 0;
    
    // 获取处理器名称
    virtual std::string getName() const = 0;
    
    // 获取处理器描述
    virtual std::string getDescription() const { return ""; }
    
    // 获取处理器支持的消息类别
    virtual MessageCategoryType getCategory() const = 0;
    
    // 获取处理器支持的消息ID
    virtual MessageIdType getId() const = 0;
};

// 创建类型安全的消息处理器
template<typename MsgType, typename Func>
class TypedMessageHandler : public MessageHandler {
    static_assert(std::is_base_of<MessageBase, MsgType>::value, "MsgType must be derived from MessageBase");
    
public:
    TypedMessageHandler(const std::string& name, Func handler)
        : name_(name), handler_(handler) {}
    
    bool handleMessage(const MessageBase& message) override {
        const MsgType* typed_msg = dynamic_cast<const MsgType*>(&message);
        if (!typed_msg) {
            return false;
        }
        
        return handler_(*typed_msg);
    }
    
    std::string getName() const override {
        return name_;
    }
    
    MessageCategoryType getCategory() const override {
        return MsgType::CATEGORY;
    }
    
    MessageIdType getId() const override {
        return MsgType::ID;
    }
    
private:
    std::string name_;
    Func handler_;
};

// 创建消息处理器的辅助函数
template<typename MsgType, typename Func>
std::unique_ptr<MessageHandler> createMessageHandler(const std::string& name, Func&& handler) {
    return std::make_unique<TypedMessageHandler<MsgType, Func>>(name, std::forward<Func>(handler));
}

} // namespace message
} // namespace next_gen
