#include "generator.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include "../../include/utils/logger.h"

extern "C" {
    #include <lua.h>
    #include <lualib.h>
    #include <lauxlib.h>
}

namespace fs = std::filesystem;
using namespace next_gen::message::generator;

MessageGenerator::MessageGenerator(const GeneratorConfig& config)
    : config_(config) {
    // 添加默认类型映射
    addTypeMapping("int8", "int8_t", FIELD_TYPE_INT8);
    addTypeMapping("uint8", "uint8_t", FIELD_TYPE_UINT8);
    addTypeMapping("int16", "int16_t", FIELD_TYPE_INT16);
    addTypeMapping("uint16", "uint16_t", FIELD_TYPE_UINT16);
    addTypeMapping("int32", "int32_t", FIELD_TYPE_INT32);
    addTypeMapping("uint32", "uint32_t", FIELD_TYPE_UINT32);
    addTypeMapping("int64", "int64_t", FIELD_TYPE_INT64);
    addTypeMapping("uint64", "uint64_t", FIELD_TYPE_UINT64);
    addTypeMapping("float", "float", FIELD_TYPE_FLOAT);
    addTypeMapping("double", "double", FIELD_TYPE_DOUBLE);
    addTypeMapping("bool", "bool", FIELD_TYPE_BOOL);
    addTypeMapping("string", "std::string", FIELD_TYPE_STRING, true, true, "<string>");
}

void MessageGenerator::addTypeMapping(
    const std::string& lua_type,
    const std::string& cpp_type,
    FieldType field_type,
    bool is_builtin,
    bool requires_include,
    const std::string& include_path
) {
    TypeMapping mapping{
        lua_type,
        cpp_type,
        field_type,
        is_builtin,
        requires_include,
        include_path
    };
    
    type_mappings_[lua_type] = mapping;
}

bool MessageGenerator::initialize() {
    // 检查输入目录是否存在
    if (!fs::exists(config_.input_dir)) {
        Logger::error("Input directory does not exist: {}", config_.input_dir);
        return false;
    }
    
    // 检查模板目录是否存在
    if (!fs::exists(config_.template_dir)) {
        Logger::error("Template directory does not exist: {}", config_.template_dir);
        return false;
    }
    
    // 创建输出目录（如果不存在）
    if (!fs::exists(config_.output_dir)) {
        if (!fs::create_directories(config_.output_dir)) {
            Logger::error("Failed to create output directory: {}", config_.output_dir);
            return false;
        }
    }
    
    Logger::info("Message generator initialized with:");
    Logger::info("  Input dir: {}", config_.input_dir);
    Logger::info("  Output dir: {}", config_.output_dir);
    Logger::info("  Template dir: {}", config_.template_dir);
    
    return true;
}

int MessageGenerator::generateAll() {
    int count = 0;
    
    // 遍历输入目录中的所有Lua文件
    for (const auto& entry : fs::directory_iterator(config_.input_dir)) {
        if (entry.path().extension() == ".lua") {
            std::string lua_file = entry.path().string();
            if (generateFile(lua_file)) {
                count++;
            }
        }
    }
    
    // 生成工厂注册文件
    if (config_.generate_factory && !message_definitions_.empty()) {
        std::string factory_file = fs::path(config_.output_dir) / "message_factory_registry.cpp";
        generateFactoryRegistration(factory_file);
    }
    
    // 生成旧系统兼容层
    if (config_.generate_legacy && !message_definitions_.empty()) {
        std::string legacy_file = fs::path(config_.output_dir) / "legacy_adapters.cpp";
        generateLegacyAdapters(legacy_file);
    }
    
    Logger::info("Generated {} message files", count);
    return count;
}

bool MessageGenerator::generateFile(const std::string& lua_file) {
    if (!loadMessageDefinitions(lua_file)) {
        Logger::error("Failed to load message definitions from {}", lua_file);
        return false;
    }
    
    fs::path lua_path(lua_file);
    std::string base_name = lua_path.stem().string();
    
    for (const auto& msg_def : message_definitions_) {
        std::string msg_name = msg_def.name;
        
        // 生成头文件
        if (config_.generate_header) {
            std::string header_file = fs::path(config_.output_dir) / 
                                      (config_.header_prefix + msg_name + config_.header_extension);
            if (!generateHeader(msg_def, header_file)) {
                Logger::error("Failed to generate header for {}", msg_name);
                return false;
            }
        }
        
        // 生成源文件
        if (config_.generate_source) {
            std::string source_file = fs::path(config_.output_dir) / 
                                      (config_.source_prefix + msg_name + config_.source_extension);
            if (!generateSource(msg_def, source_file)) {
                Logger::error("Failed to generate source for {}", msg_name);
                return false;
            }
        }
        
        Logger::info("Generated message: {}", msg_name);
    }
    
    return true;
}

// Lua回调函数，用于获取消息定义
static int lua_get_message_definition(lua_State* L) {
    // 获取消息定义表
    luaL_checktype(L, 1, LUA_TTABLE);
    
    // 创建结果表
    lua_newtable(L);
    
    // 处理每个字段
    lua_pushnil(L);  // 第一个键
    while (lua_next(L, 1) != 0) {
        // 键在索引 -2，值在索引 -1
        if (lua_isstring(L, -2)) {
            const char* key = lua_tostring(L, -2);
            
            // 复制键值对到结果表
            lua_pushvalue(L, -2);  // 复制键
            lua_pushvalue(L, -2);  // 复制值
            lua_settable(L, -4);   // 设置到结果表
        }
        
        // 移除值，保留键用于下一次迭代
        lua_pop(L, 1);
    }
    
    return 1;  // 返回结果表
}

bool MessageGenerator::loadMessageDefinitions(const std::string& lua_file) {
    message_definitions_.clear();
    
    // 创建Lua状态
    lua_State* L = luaL_newstate();
    if (!L) {
        Logger::error("Failed to create Lua state");
        return false;
    }
    
    // 加载标准库
    luaL_openlibs(L);
    
    // 注册辅助函数
    lua_register(L, "get_message_definition", lua_get_message_definition);
    
    // 加载Lua文件
    if (luaL_dofile(L, lua_file.c_str()) != 0) {
        Logger::error("Failed to load Lua file {}: {}", lua_file, lua_tostring(L, -1));
        lua_close(L);
        return false;
    }
    
    // 获取消息定义表
    lua_getglobal(L, "messages");
    if (!lua_istable(L, -1)) {
        Logger::error("No 'messages' table found in {}", lua_file);
        lua_close(L);
        return false;
    }
    
    // 遍历消息定义
    lua_pushnil(L);  // 第一个键
    while (lua_next(L, -2) != 0) {
        // 键在索引 -2，值在索引 -1
        if (lua_isstring(L, -2) && lua_istable(L, -1)) {
            const char* msg_name = lua_tostring(L, -2);
            
            // 调用get_message_definition函数获取消息定义
            lua_pushcfunction(L, lua_get_message_definition);
            lua_pushvalue(L, -2);  // 复制消息定义表
            if (lua_pcall(L, 1, 1, 0) != 0) {
                Logger::error("Failed to get message definition for {}: {}", 
                             msg_name, lua_tostring(L, -1));
                lua_pop(L, 1);
                continue;
            }
            
            // 解析消息定义
            MessageDefinition msg_def;
            msg_def.name = msg_name;
            
            // 获取消息类别
            lua_getfield(L, -1, "category");
            if (lua_isnumber(L, -1)) {
                msg_def.category = (uint16_t)lua_tointeger(L, -1);
            }
            lua_pop(L, 1);
            
            // 获取消息ID
            lua_getfield(L, -1, "id");
            if (lua_isnumber(L, -1)) {
                msg_def.id = (uint16_t)lua_tointeger(L, -1);
            }
            lua_pop(L, 1);
            
            // 获取消息描述
            lua_getfield(L, -1, "desc");
            if (lua_isstring(L, -1)) {
                msg_def.description = lua_tostring(L, -1);
            }
            lua_pop(L, 1);
            
            // 获取消息版本
            lua_getfield(L, -1, "version");
            if (lua_isnumber(L, -1)) {
                msg_def.version = (uint16_t)lua_tointeger(L, -1);
            } else {
                msg_def.version = 1;  // 默认版本
            }
            lua_pop(L, 1);
            
            // 获取字段定义
            lua_getfield(L, -1, "fields");
            if (lua_istable(L, -1)) {
                // 遍历字段定义
                lua_pushnil(L);  // 第一个键
                while (lua_next(L, -2) != 0) {
                    // 键在索引 -2，值在索引 -1
                    if (lua_isstring(L, -2) && lua_istable(L, -1)) {
                        const char* field_name = lua_tostring(L, -2);
                        
                        MessageDefinition::Field field;
                        field.name = field_name;
                        
                        // 获取字段类型
                        lua_getfield(L, -1, "type");
                        if (lua_isstring(L, -1)) {
                            field.type = lua_tostring(L, -1);
                        }
                        lua_pop(L, 1);
                        
                        // 检查是否是向量类型
                        field.is_vector = false;
                        if (field.type.find("array") == 0) {
                            field.is_vector = true;
                            // 提取数组元素类型
                            size_t pos = field.type.find("<");
                            if (pos != std::string::npos && field.type.back() == '>') {
                                field.type = field.type.substr(pos + 1, field.type.length() - pos - 2);
                            }
                        }
                        
                        // 获取字段描述
                        lua_getfield(L, -1, "desc");
                        if (lua_isstring(L, -1)) {
                            field.description = lua_tostring(L, -1);
                        }
                        lua_pop(L, 1);
                        
                        // 获取字段是否必需
                        lua_getfield(L, -1, "required");
                        if (lua_isboolean(L, -1)) {
                            field.is_required = lua_toboolean(L, -1) != 0;
                        } else {
                            field.is_required = true;  // 默认为必需
                        }
                        lua_pop(L, 1);
                        
                        // 获取默认值
                        lua_getfield(L, -1, "default");
                        if (!lua_isnil(L, -1)) {
                            if (lua_isstring(L, -1)) {
                                field.default_value = lua_tostring(L, -1);
                            } else if (lua_isnumber(L, -1)) {
                                field.default_value = std::to_string(lua_tonumber(L, -1));
                            } else if (lua_isboolean(L, -1)) {
                                field.default_value = lua_toboolean(L, -1) ? "true" : "false";
                            }
                        }
                        lua_pop(L, 1);
                        
                        msg_def.fields.push_back(field);
                    }
                    
                    lua_pop(L, 1);
                }
            }
            lua_pop(L, 1);
            
            message_definitions_.push_back(msg_def);
            
            // 移除结果表
            lua_pop(L, 1);
        }
        
        lua_pop(L, 1);
    }
    
    lua_close(L);
    return !message_definitions_.empty();
}
