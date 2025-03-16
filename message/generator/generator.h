#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include "../include/types.h"

namespace next_gen {
namespace message {
namespace generator {

/**
 * @brief 消息生成器配置
 */
struct GeneratorConfig {
    // 输入和输出路径
    std::string input_dir;          // Lua消息定义的输入目录
    std::string output_dir;         // 生成的C++文件的输出目录
    std::string template_dir;       // 模板文件目录
    
    // 生成选项
    bool generate_header = true;    // 生成头文件
    bool generate_source = true;    // 生成源文件
    bool generate_factory = true;   // 生成工厂注册
    bool generate_legacy = true;    // 生成旧系统兼容层
    bool verbose = false;           // 是否输出详细信息
    
    // 文件名配置
    std::string header_extension = ".h";
    std::string source_extension = ".cpp";
    std::string header_prefix = "msg_";
    std::string source_prefix = "msg_";
    
    // 命名空间配置
    std::string base_namespace = "next_gen::message";
};

/**
 * @brief 类型映射信息
 */
struct TypeMapping {
    std::string lua_type;           // Lua类型名称
    std::string cpp_type;           // 对应的C++类型名称
    FieldType field_type;           // 字段类型枚举
    bool is_builtin;                // 是否是内置类型
    bool requires_include;          // 是否需要额外包含头文件
    std::string include_path;       // 包含的头文件路径
};

/**
 * @brief 消息定义信息
 */
struct MessageDefinition {
    std::string name;               // 消息名称
    uint16_t category;              // 消息类别
    uint16_t id;                    // 消息ID
    std::string description;        // 消息描述
    uint16_t version;               // 消息版本
    
    struct Field {
        std::string name;           // 字段名称
        std::string type;           // 字段类型
        std::string description;    // 字段描述
        bool is_vector;             // 是否是向量类型
        bool is_required;           // 是否是必需字段
        std::string default_value;  // 默认值
    };
    
    std::vector<Field> fields;      // 字段列表
};

/**
 * @brief 消息生成器
 * 
 * 将Lua消息定义转换为C++代码
 */
class MessageGenerator {
public:
    /**
     * @brief 构造函数
     * 
     * @param config 生成器配置
     */
    explicit MessageGenerator(const GeneratorConfig& config);
    
    /**
     * @brief 初始化生成器
     * 
     * @return 是否初始化成功
     */
    bool initialize();
    
    /**
     * @brief 生成所有消息
     * 
     * @return 生成的消息数量
     */
    int generateAll();
    
    /**
     * @brief 生成指定的消息定义文件
     * 
     * @param lua_file Lua消息定义文件路径
     * @return 是否成功生成
     */
    bool generateFile(const std::string& lua_file);
    
    /**
     * @brief 添加类型映射
     * 
     * @param lua_type Lua类型名称
     * @param cpp_type C++类型名称
     * @param field_type 字段类型枚举
     * @param is_builtin 是否是内置类型
     * @param requires_include 是否需要额外包含头文件
     * @param include_path 包含的头文件路径
     */
    void addTypeMapping(
        const std::string& lua_type,
        const std::string& cpp_type,
        FieldType field_type,
        bool is_builtin = true,
        bool requires_include = false,
        const std::string& include_path = ""
    );
    
    /**
     * @brief 获取消息定义列表
     * 
     * @return 消息定义列表
     */
    const std::vector<MessageDefinition>& getMessageDefinitions() const {
        return message_definitions_;
    }
    
private:
    // 从Lua文件加载消息定义
    bool loadMessageDefinitions(const std::string& lua_file);
    
    // 生成头文件
    bool generateHeader(const MessageDefinition& message_def, const std::string& output_file);
    
    // 生成源文件
    bool generateSource(const MessageDefinition& message_def, const std::string& output_file);
    
    // 生成工厂注册
    bool generateFactoryRegistration(const std::string& output_file);
    
    // 生成旧系统兼容层
    bool generateLegacyAdapters(const std::string& output_file);
    
    // 获取字段的C++类型
    std::string getCppType(const std::string& lua_type, bool is_vector);
    
    // 获取字段的序列化代码
    std::string getSerializeCode(const std::string& field_name, const std::string& field_type, bool is_vector);
    
    // 获取字段的反序列化代码
    std::string getDeserializeCode(const std::string& field_name, const std::string& field_type, bool is_vector);
    
    // 获取字段大小计算代码
    std::string getSizeCode(const std::string& field_name, const std::string& field_type, bool is_vector);
    
    // 生成器配置
    GeneratorConfig config_;
    
    // 类型映射表
    std::unordered_map<std::string, TypeMapping> type_mappings_;
    
    // 加载的消息定义
    std::vector<MessageDefinition> message_definitions_;
};

} // namespace generator
} // namespace message
} // namespace next_gen
