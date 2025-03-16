#include "generator.h"
#include "template_engine.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cctype>
#include <algorithm>
#include "../../include/utils/logger.h"

namespace fs = std::filesystem;
using namespace next_gen::message::generator;

// 辅助函数：转换字段名为驼峰命名
static std::string toCamelCase(const std::string& name) {
    std::string result = name;
    if (!result.empty()) {
        result[0] = std::toupper(result[0]);
    }
    return result;
}

// 辅助函数：转换字段名为小写
static std::string toLowerCase(const std::string& name) {
    std::string result = name;
    std::transform(result.begin(), result.end(), result.begin(), 
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

bool MessageGenerator::generateHeader(const MessageDefinition& message_def, const std::string& output_file) {
    // 加载头文件模板
    TemplateEngine engine;
    std::string template_file = fs::path(config_.template_dir) / "message_header.template";
    if (!engine.loadFromFile(template_file)) {
        Logger::error("Failed to load header template from {}", template_file);
        return false;
    }
    
    // 设置基本变量
    engine.setVariable("message_name", message_def.name);
    engine.setVariable("message_class_name", message_def.name + "Message");
    engine.setVariable("message_description", message_def.description);
    engine.setVariable("message_category", std::to_string(message_def.category));
    engine.setVariable("message_id", std::to_string(message_def.id));
    engine.setVariable("message_version", std::to_string(message_def.version));
    
    // 检查是否需要额外的包含
    bool has_additional_includes = false;
    std::stringstream includes;
    std::set<std::string> include_set;
    
    for (const auto& field : message_def.fields) {
        auto it = type_mappings_.find(field.type);
        if (it != type_mappings_.end() && it->second.requires_include) {
            if (include_set.find(it->second.include_path) == include_set.end()) {
                includes << "#include " << it->second.include_path << std::endl;
                include_set.insert(it->second.include_path);
                has_additional_includes = true;
            }
        }
    }
    
    engine.setCondition("has_additional_includes", has_additional_includes);
    engine.setVariable("additional_includes", includes.str());
    
    // 设置字段循环
    std::vector<MessageDefinition::Field> fields = message_def.fields;
    engine.setLoop("field", fields.size(), 
                 [this, &fields](const std::string& loop_body, int index) -> std::string {
        TemplateEngine field_engine;
        field_engine.loadFromString(loop_body);
        
        const auto& field = fields[index];
        std::string field_name = field.name;
        std::string field_name_capitalized = toCamelCase(field_name);
        std::string field_name_lower = toLowerCase(field_name);
        std::string field_cpp_type = getCppType(field.type, field.is_vector);
        
        field_engine.setVariable("field_name", field_name);
        field_engine.setVariable("field_name_capitalized", field_name_capitalized);
        field_engine.setVariable("field_name_lower", field_name_lower);
        field_engine.setVariable("field_cpp_type", field_cpp_type);
        field_engine.setVariable("field_description", field.description);
        field_engine.setVariable("field_is_vector", field.is_vector ? "true" : "false");
        field_engine.setCondition("field_is_vector", field.is_vector);
        field_engine.setCondition("field_has_default", !field.default_value.empty());
        field_engine.setVariable("field_default_value", field.default_value);
        
        return field_engine.render();
    });
    
    // 渲染模板并写入文件
    std::string content = engine.render();
    
    std::ofstream out(output_file);
    if (!out.is_open()) {
        Logger::error("Failed to open output file: {}", output_file);
        return false;
    }
    
    out << content;
    out.close();
    
    Logger::info("Generated header file: {}", output_file);
    return true;
}

bool MessageGenerator::generateSource(const MessageDefinition& message_def, const std::string& output_file) {
    // 加载源文件模板
    TemplateEngine engine;
    std::string template_file = fs::path(config_.template_dir) / "message_source.template";
    if (!engine.loadFromFile(template_file)) {
        Logger::error("Failed to load source template from {}", template_file);
        return false;
    }
    
    // 设置基本变量
    std::string header_name = config_.header_prefix + message_def.name + config_.header_extension;
    std::string header_path = fs::path("message/generated") / header_name;
    
    engine.setVariable("header_include_path", header_path);
    engine.setVariable("message_class_name", message_def.name + "Message");
    
    // 设置字段循环
    std::vector<MessageDefinition::Field> fields = message_def.fields;
    engine.setLoop("field", fields.size(), 
                 [this, &fields](const std::string& loop_body, int index) -> std::string {
        TemplateEngine field_engine;
        field_engine.loadFromString(loop_body);
        
        const auto& field = fields[index];
        std::string field_name = field.name;
        std::string field_name_lower = toLowerCase(field_name);
        std::string field_cpp_type = getCppType(field.type, field.is_vector);
        
        // 获取序列化相关代码
        std::string size_code = getSizeCode(field_name_lower, field.type, field.is_vector);
        std::string serialize_code = getSerializeCode(field_name_lower, field.type, field.is_vector);
        std::string deserialize_code = getDeserializeCode(field_name_lower, field.type, field.is_vector);
        
        // 获取toString相关代码
        std::string to_string_code;
        if (field.type == "string") {
            to_string_code = field.is_vector 
                ? "ss << " + field_name_lower + "[i]"
                : "ss << " + field_name_lower;
        } else if (field.type == "bool") {
            to_string_code = field.is_vector 
                ? "ss << (" + field_name_lower + "[i] ? \"true\" : \"false\")"
                : "ss << (" + field_name_lower + " ? \"true\" : \"false\")";
        } else {
            to_string_code = field.is_vector 
                ? "ss << " + field_name_lower + "[i]"
                : "ss << " + field_name_lower;
        }
        
        // 获取字段类型枚举
        std::string field_type_enum = "FIELD_TYPE_CUSTOM";
        auto it = type_mappings_.find(field.type);
        if (it != type_mappings_.end()) {
            field_type_enum = "FIELD_TYPE_" + field.type;
        }
        
        field_engine.setVariable("field_name", field_name);
        field_engine.setVariable("field_name_lower", field_name_lower);
        field_engine.setVariable("field_type_enum", field_type_enum);
        field_engine.setVariable("field_type_name", field.type);
        field_engine.setVariable("field_description", field.description);
        field_engine.setVariable("field_is_vector_bool", field.is_vector ? "true" : "false");
        field_engine.setVariable("field_is_required_bool", field.is_required ? "true" : "false");
        field_engine.setVariable("field_size_code", size_code);
        field_engine.setVariable("field_serialize_code", serialize_code);
        field_engine.setVariable("field_deserialize_code", deserialize_code);
        field_engine.setVariable("field_to_string_code", to_string_code);
        field_engine.setCondition("field_is_vector", field.is_vector);
        field_engine.setCondition("field_has_default", !field.default_value.empty());
        field_engine.setVariable("field_default_value", field.default_value);
        
        return field_engine.render();
    });
    
    // 渲染模板并写入文件
    std::string content = engine.render();
    
    std::ofstream out(output_file);
    if (!out.is_open()) {
        Logger::error("Failed to open output file: {}", output_file);
        return false;
    }
    
    out << content;
    out.close();
    
    Logger::info("Generated source file: {}", output_file);
    return true;
}

bool MessageGenerator::generateFactoryRegistration(const std::string& output_file) {
    // 加载工厂注册模板
    TemplateEngine engine;
    std::string template_file = fs::path(config_.template_dir) / "factory_registration.template";
    if (!engine.loadFromFile(template_file)) {
        Logger::error("Failed to load factory registration template from {}", template_file);
        return false;
    }
    
    // 设置消息循环
    engine.setLoop("message", message_definitions_.size(), 
                 [this](const std::string& loop_body, int index) -> std::string {
        TemplateEngine msg_engine;
        msg_engine.loadFromString(loop_body);
        
        const auto& msg_def = message_definitions_[index];
        std::string header_name = config_.header_prefix + msg_def.name + config_.header_extension;
        std::string header_path = fs::path("message/generated") / header_name;
        
        msg_engine.setVariable("message_class_name", msg_def.name + "Message");
        msg_engine.setVariable("message_header_path", header_path);
        
        return msg_engine.render();
    });
    
    // 渲染模板并写入文件
    std::string content = engine.render();
    
    std::ofstream out(output_file);
    if (!out.is_open()) {
        Logger::error("Failed to open output file: {}", output_file);
        return false;
    }
    
    out << content;
    out.close();
    
    Logger::info("Generated factory registration file: {}", output_file);
    return true;
}

bool MessageGenerator::generateLegacyAdapters(const std::string& output_file) {
    // 加载旧系统适配器模板
    TemplateEngine engine;
    std::string template_file = fs::path(config_.template_dir) / "legacy_adapters.template";
    if (!engine.loadFromFile(template_file)) {
        Logger::error("Failed to load legacy adapters template from {}", template_file);
        return false;
    }
    
    // 设置消息循环
    engine.setLoop("message", message_definitions_.size(), 
                 [this](const std::string& loop_body, int index) -> std::string {
        TemplateEngine msg_engine;
        msg_engine.loadFromString(loop_body);
        
        const auto& msg_def = message_definitions_[index];
        std::string header_name = config_.header_prefix + msg_def.name + config_.header_extension;
        std::string header_path = fs::path("message/generated") / header_name;
        
        msg_engine.setVariable("message_name", msg_def.name);
        msg_engine.setVariable("message_class_name", msg_def.name + "Message");
        msg_engine.setVariable("message_header_path", header_path);
        
        return msg_engine.render();
    });
    
    // 渲染模板并写入文件
    std::string content = engine.render();
    
    std::ofstream out(output_file);
    if (!out.is_open()) {
        Logger::error("Failed to open output file: {}", output_file);
        return false;
    }
    
    out << content;
    out.close();
    
    Logger::info("Generated legacy adapters file: {}", output_file);
    return true;
}

std::string MessageGenerator::getCppType(const std::string& lua_type, bool is_vector) {
    auto it = type_mappings_.find(lua_type);
    if (it != type_mappings_.end()) {
        return it->second.cpp_type;
    }
    
    // 未知类型，假设是自定义消息类型
    return lua_type + "Message";
}

std::string MessageGenerator::getSerializeCode(const std::string& field_name, const std::string& field_type, bool is_vector) {
    std::stringstream ss;
    
    if (is_vector) {
        ss << "{\n";
        ss << "    // 写入数组大小\n";
        ss << "    uint16_t size = static_cast<uint16_t>(" << field_name << ".size());\n";
        ss << "    stream.write(size);\n";
        ss << "    \n";
        ss << "    // 写入数组元素\n";
        ss << "    for (const auto& item : " << field_name << ") {\n";
        
        auto it = type_mappings_.find(field_type);
        if (it != type_mappings_.end() && it->second.is_builtin) {
            // 内置类型
            ss << "        stream.write(item);\n";
        } else {
            // 自定义类型
            ss << "        item.serialize(stream);\n";
        }
        
        ss << "    }\n";
        ss << "}";
    } else {
        auto it = type_mappings_.find(field_type);
        if (it != type_mappings_.end() && it->second.is_builtin) {
            // 内置类型
            ss << "stream.write(" << field_name << ");";
        } else {
            // 自定义类型
            ss << field_name << ".serialize(stream);";
        }
    }
    
    return ss.str();
}

std::string MessageGenerator::getDeserializeCode(const std::string& field_name, const std::string& field_type, bool is_vector) {
    std::stringstream ss;
    
    if (is_vector) {
        ss << "{\n";
        ss << "    // 读取数组大小\n";
        ss << "    uint16_t size;\n";
        ss << "    stream.read(size);\n";
        ss << "    \n";
        ss << "    // 调整数组大小\n";
        ss << "    " << field_name << ".resize(size);\n";
        ss << "    \n";
        ss << "    // 读取数组元素\n";
        ss << "    for (uint16_t i = 0; i < size; ++i) {\n";
        
        auto it = type_mappings_.find(field_type);
        if (it != type_mappings_.end() && it->second.is_builtin) {
            // 内置类型
            ss << "        stream.read(" << field_name << "[i]);\n";
        } else {
            // 自定义类型
            ss << "        " << field_name << "[i].deserialize(stream);\n";
        }
        
        ss << "    }\n";
        ss << "}";
    } else {
        auto it = type_mappings_.find(field_type);
        if (it != type_mappings_.end() && it->second.is_builtin) {
            // 内置类型
            ss << "stream.read(" << field_name << ");";
        } else {
            // 自定义类型
            ss << field_name << ".deserialize(stream);";
        }
    }
    
    return ss.str();
}

std::string MessageGenerator::getSizeCode(const std::string& field_name, const std::string& field_type, bool is_vector) {
    std::stringstream ss;
    
    if (is_vector) {
        ss << "{\n";
        ss << "    // 数组大小字段\n";
        ss << "    size += sizeof(uint16_t);\n";
        ss << "    \n";
        ss << "    // 数组元素大小\n";
        ss << "    for (const auto& item : " << field_name << ") {\n";
        
        auto it = type_mappings_.find(field_type);
        if (it != type_mappings_.end() && it->second.is_builtin) {
            // 内置类型
            if (field_type == "string") {
                ss << "        size += sizeof(uint16_t) + item.size();\n";
            } else {
                ss << "        size += sizeof(" << it->second.cpp_type << ");\n";
            }
        } else {
            // 自定义类型
            ss << "        size += item.getSerializedSize();\n";
        }
        
        ss << "    }\n";
        ss << "}";
    } else {
        auto it = type_mappings_.find(field_type);
        if (it != type_mappings_.end() && it->second.is_builtin) {
            // 内置类型
            if (field_type == "string") {
                ss << "size += sizeof(uint16_t) + " << field_name << ".size();";
            } else {
                ss << "size += sizeof(" << it->second.cpp_type << ");";
            }
        } else {
            // 自定义类型
            ss << "size += " << field_name << ".getSerializedSize();";
        }
    }
    
    return ss.str();
}
