#pragma once

#include <memory>
#include <functional>
#include <unordered_map>
#include "../message_base.h"
#include "../../../include/utils/logger.h"

namespace next_gen {
namespace message {
namespace legacy {

/**
 * @brief 旧消息系统适配器
 * 
 * 提供新旧消息系统之间的转换功能，确保平滑过渡
 */
class MessageAdapter {
public:
    /**
     * @brief 从旧格式消息转换为新格式消息
     * 
     * @param old_msg_ptr 指向旧格式消息的指针
     * @param category 消息类别
     * @param id 消息ID
     * @return 新格式消息对象
     */
    static std::unique_ptr<MessageBase> fromLegacyFormat(void* old_msg_ptr, MessageCategoryType category, MessageIdType id);
    
    /**
     * @brief 将新格式消息转换为旧格式
     * 
     * @param new_msg 新格式消息
     * @return 指向旧格式消息的指针（需要调用者释放）
     */
    static void* toLegacyFormat(const MessageBase& new_msg);
    
    /**
     * @brief 注册消息转换器
     * 
     * @tparam OldMsgType 旧消息类型
     * @tparam NewMsgType 新消息类型（必须是MessageBase的派生类）
     */
    template<typename OldMsgType, typename NewMsgType>
    static void registerConverter() {
        static_assert(std::is_base_of<MessageBase, NewMsgType>::value, 
                     "NewMsgType must derive from MessageBase");
        
        // 创建旧到新的转换函数
        auto toNewFn = [](void* old_ptr) -> std::unique_ptr<MessageBase> {
            if (!old_ptr) return nullptr;
            
            auto* old_msg = static_cast<OldMsgType*>(old_ptr);
            auto new_msg = std::make_unique<NewMsgType>();
            
            // 调用类型特化的转换函数
            bool success = convertFromLegacy(*old_msg, *new_msg);
            if (success) {
                return new_msg;
            }
            
            return nullptr;
        };
        
        // 创建新到旧的转换函数
        auto toOldFn = [](const MessageBase& new_msg) -> void* {
            const auto* typed_new = dynamic_cast<const NewMsgType*>(&new_msg);
            if (!typed_new) return nullptr;
            
            auto* old_msg = new OldMsgType();
            
            // 调用类型特化的转换函数
            bool success = convertToLegacy(*typed_new, *old_msg);
            if (success) {
                return old_msg;
            }
            
            delete old_msg;
            return nullptr;
        };
        
        // 注册转换器
        auto category = NewMsgType::CATEGORY;
        auto id = NewMsgType::ID;
        auto key = std::make_pair(category, id);
        
        fromLegacyConverters_[key] = toNewFn;
        toLegacyConverters_[key] = toOldFn;
        
        Logger::info("Registered legacy converter for [{}, {}] - {}", 
                    category, id, NewMsgType::NAME);
    }
    
private:
    // 转换器映射表
    static std::unordered_map<
        std::pair<MessageCategoryType, MessageIdType>,
        std::function<std::unique_ptr<MessageBase>(void*)>
    > fromLegacyConverters_;
    
    static std::unordered_map<
        std::pair<MessageCategoryType, MessageIdType>,
        std::function<void*(const MessageBase&)>
    > toLegacyConverters_;
    
    // 默认的转换函数（需要特化）
    template<typename OldMsg, typename NewMsg>
    static bool convertFromLegacy(const OldMsg& old_msg, NewMsg& new_msg) {
        // 这是一个默认实现，需要为每个消息类型提供特化版本
        Logger::warning("No converter implemented from legacy {} to new {}",
                       typeid(OldMsg).name(), typeid(NewMsg).name());
        return false;
    }
    
    template<typename NewMsg, typename OldMsg>
    static bool convertToLegacy(const NewMsg& new_msg, OldMsg& old_msg) {
        // 这是一个默认实现，需要为每个消息类型提供特化版本
        Logger::warning("No converter implemented from new {} to legacy {}",
                       typeid(NewMsg).name(), typeid(OldMsg).name());
        return false;
    }
};

/**
 * @brief 旧消息处理器适配器
 * 
 * 将旧系统的消息处理函数包装成新系统的MessageHandler
 */
class LegacyHandlerAdapter : public MessageHandler {
public:
    /**
     * @brief 构造函数
     * 
     * @param name 处理器名称
     * @param category 消息类别
     * @param id 消息ID
     * @param handler 旧处理器函数
     */
    LegacyHandlerAdapter(
        const std::string& name,
        MessageCategoryType category,
        MessageIdType id,
        std::function<bool(void*)> handler
    );
    
    // 实现MessageHandler接口
    bool handleMessage(const MessageBase& message) override;
    std::string getName() const override;
    MessageCategoryType getCategory() const override;
    MessageIdType getId() const override;
    
private:
    std::string name_;
    MessageCategoryType category_;
    MessageIdType id_;
    std::function<bool(void*)> legacy_handler_;
};

/**
 * @brief 创建旧处理器适配器的辅助函数
 * 
 * @param name 处理器名称
 * @param category 消息类别
 * @param id 消息ID
 * @param handler 旧处理器函数
 * @return 新处理器对象
 */
std::unique_ptr<MessageHandler> createLegacyHandler(
    const std::string& name,
    MessageCategoryType category,
    MessageIdType id,
    std::function<bool(void*)> handler
);

/**
 * @brief 旧消息转换注册辅助宏
 */
#define REGISTER_LEGACY_CONVERTER(OldMsgType, NewMsgType) \
    static bool _reg_conv_##OldMsgType##_##NewMsgType = []() { \
        MessageAdapter::registerConverter<OldMsgType, NewMsgType>(); \
        return true; \
    }();

} // namespace legacy
} // namespace message
} // namespace next_gen
