#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <type_traits>
#include "../../include/core/common.h"
#include "../../include/utils/byte_stream.h"

namespace next_gen {
namespace message {

// 基本类型定义，确保平台一致性
using MessageIdType = uint16_t;
using MessageCategoryType = uint16_t;
using MessageSizeType = uint32_t;

// 消息类别常量定义
enum MessageCategory : MessageCategoryType {
    MSG_CATEGORY_SYSTEM = 0,
    MSG_CATEGORY_PLAYER = 1,
    MSG_CATEGORY_ITEM = 2,
    MSG_CATEGORY_GUILD = 3,
    MSG_CATEGORY_SCENE = 4,
    MSG_CATEGORY_COMBAT = 5,
    MSG_CATEGORY_CHAT = 6,
    MSG_CATEGORY_AUTH = 7,
    MSG_CATEGORY_LOGIN = 8,
    MSG_CATEGORY_TRADE = 9,
    MSG_CATEGORY_DATABASE = 10,
    // 更多类别...
};

// 字段类型定义
enum FieldType {
    FIELD_TYPE_INT8 = 0,
    FIELD_TYPE_UINT8 = 1,
    FIELD_TYPE_INT16 = 2,
    FIELD_TYPE_UINT16 = 3,
    FIELD_TYPE_INT32 = 4,
    FIELD_TYPE_UINT32 = 5,
    FIELD_TYPE_INT64 = 6,
    FIELD_TYPE_UINT64 = 7,
    FIELD_TYPE_FLOAT = 8,
    FIELD_TYPE_DOUBLE = 9,
    FIELD_TYPE_BOOL = 10,
    FIELD_TYPE_STRING = 11,
    FIELD_TYPE_VECTOR = 12,
    FIELD_TYPE_MAP = 13,
    FIELD_TYPE_MESSAGE = 14,
    FIELD_TYPE_CUSTOM = 15,
};

// 类型特征，用于类型安全和序列化
template<typename T>
struct TypeTraits {
    static constexpr bool is_serializable = false;
    static constexpr const char* name = "unknown";
    static constexpr FieldType field_type = FIELD_TYPE_CUSTOM;
    
    static MessageSizeType serialized_size(const T& value);
    static void serialize(ByteStream& stream, const T& value);
    static void deserialize(ByteStream& stream, T& value);
};

// 字段信息结构，用于反射
struct FieldInfo {
    std::string name;
    FieldType type;
    std::string type_name;
    std::string description;
    bool is_vector;
    bool is_required;
    
    FieldInfo(
        const std::string& name_,
        FieldType type_,
        const std::string& type_name_,
        const std::string& description_ = "",
        bool is_vector_ = false,
        bool is_required_ = true
    ) : name(name_), type(type_), type_name(type_name_), 
        description(description_), is_vector(is_vector_), is_required(is_required_) {}
};

// 消息信息结构，用于注册
struct MessageInfo {
    MessageCategoryType category;
    MessageIdType id;
    std::string name;
    std::string description;
    uint16_t version;
    std::vector<FieldInfo> fields;
    
    MessageInfo(
        MessageCategoryType category_,
        MessageIdType id_,
        const std::string& name_,
        const std::string& description_ = "",
        uint16_t version_ = 1
    ) : category(category_), id(id_), name(name_),
        description(description_), version(version_) {}
        
    void addField(const FieldInfo& field) {
        fields.push_back(field);
    }
};

} // namespace message
} // namespace next_gen
