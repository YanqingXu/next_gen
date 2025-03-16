#pragma once

#include <unordered_map>
#include <functional>
#include "message_base.h"
#include "../../include/core/singleton.h"

namespace next_gen {
namespace message {

/**
 * @brief 默认消息工厂实现
 * 
 * 负责创建和管理所有应用层消息类型
 */
class DefaultMessageFactory : public MessageFactory, public Singleton<DefaultMessageFactory> {
    friend class Singleton<DefaultMessageFactory>;
    
public:
    // 创建消息
    std::unique_ptr<MessageBase> createMessage(MessageCategoryType category, MessageIdType id) override {
        auto key = makeKey(category, id);
        auto it = creators_.find(key);
        if (it != creators_.end()) {
            return it->second();
        }
        return nullptr;
    }
    
    // 检查消息类型是否已注册
    bool isMessageTypeRegistered(MessageCategoryType category, MessageIdType id) const {
        auto key = makeKey(category, id);
        return creators_.find(key) != creators_.end();
    }
    
    // 获取所有已注册的消息信息
    std::vector<MessageInfo> getAllMessageInfo() const {
        std::vector<MessageInfo> result;
        for (const auto& pair : creators_) {
            auto msg = pair.second();
            if (msg) {
                result.push_back(msg->getMessageInfo());
            }
        }
        return result;
    }
    
protected:
    // 注册消息创建器
    void registerMessageCreator(
        MessageCategoryType category, 
        MessageIdType id, 
        std::function<std::unique_ptr<MessageBase>()> creator) override {
        auto key = makeKey(category, id);
        creators_[key] = creator;
    }
    
private:
    DefaultMessageFactory() = default;
    
    // 创建类别和ID的组合键
    static uint32_t makeKey(MessageCategoryType category, MessageIdType id) {
        return (static_cast<uint32_t>(category) << 16) | static_cast<uint32_t>(id);
    }
    
    // 消息创建器映射表
    std::unordered_map<uint32_t, std::function<std::unique_ptr<MessageBase>()>> creators_;
};

/**
 * @brief 消息注册辅助宏
 * 
 * 简化消息类型的注册过程
 */
#define REGISTER_MESSAGE_TYPE(MessageType) \
    static bool _reg_##MessageType = []() { \
        DefaultMessageFactory::instance().registerMessageType<MessageType>(); \
        return true; \
    }();

} // namespace message
} // namespace next_gen
