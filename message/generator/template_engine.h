#pragma once

#include <string>
#include <map>
#include <functional>

namespace next_gen {
namespace message {
namespace generator {

/**
 * @brief 简单的模板引擎
 * 
 * 支持变量替换和条件语句
 */
class TemplateEngine {
public:
    /**
     * @brief 构造函数
     */
    TemplateEngine();
    
    /**
     * @brief 从文件加载模板
     * 
     * @param template_file 模板文件路径
     * @return 是否加载成功
     */
    bool loadFromFile(const std::string& template_file);
    
    /**
     * @brief 从字符串加载模板
     * 
     * @param template_content 模板内容
     */
    void loadFromString(const std::string& template_content);
    
    /**
     * @brief 设置变量
     * 
     * @param name 变量名
     * @param value 变量值
     */
    void setVariable(const std::string& name, const std::string& value);
    
    /**
     * @brief 设置列表变量
     * 
     * @param name 列表名
     * @param items 列表项
     * @param separator 分隔符
     */
    void setList(const std::string& name, 
                const std::vector<std::string>& items,
                const std::string& separator = "\n");
    
    /**
     * @brief 设置条件
     * 
     * @param name 条件名
     * @param value 条件值
     */
    void setCondition(const std::string& name, bool value);
    
    /**
     * @brief 设置循环替换函数
     * 
     * @param loop_name 循环名
     * @param item_count 项目数量
     * @param handler 处理函数，接收循环体模板和索引，返回替换后的字符串
     */
    void setLoop(const std::string& loop_name, 
                int item_count,
                std::function<std::string(const std::string&, int)> handler);
    
    /**
     * @brief 渲染模板
     * 
     * @return 渲染后的内容
     */
    std::string render() const;
    
private:
    std::string template_content_;
    std::map<std::string, std::string> variables_;
    std::map<std::string, bool> conditions_;
    std::map<std::string, std::function<std::string(const std::string&)>> loops_;
    
    // 替换变量
    std::string replaceVariables(const std::string& content) const;
    
    // 处理条件语句
    std::string processConditions(const std::string& content) const;
    
    // 处理循环语句
    std::string processLoops(const std::string& content) const;
    
    // 提取循环体
    std::string extractLoopBody(const std::string& content, 
                               const std::string& loop_name,
                               size_t start_pos,
                               size_t& end_pos) const;
};

} // namespace generator
} // namespace message
} // namespace next_gen
