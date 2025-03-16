#include "template_engine.h"
#include <fstream>
#include <sstream>
#include <regex>
#include "../../include/utils/logger.h"

namespace next_gen {
namespace message {
namespace generator {

TemplateEngine::TemplateEngine() {}

bool TemplateEngine::loadFromFile(const std::string& template_file) {
    std::ifstream file(template_file);
    if (!file.is_open()) {
        Logger::error("Failed to open template file: {}", template_file);
        return false;
    }
    
    std::stringstream ss;
    ss << file.rdbuf();
    template_content_ = ss.str();
    
    return true;
}

void TemplateEngine::loadFromString(const std::string& template_content) {
    template_content_ = template_content;
}

void TemplateEngine::setVariable(const std::string& name, const std::string& value) {
    variables_[name] = value;
}

void TemplateEngine::setList(const std::string& name, 
                           const std::vector<std::string>& items,
                           const std::string& separator) {
    std::stringstream ss;
    for (size_t i = 0; i < items.size(); ++i) {
        ss << items[i];
        if (i < items.size() - 1) {
            ss << separator;
        }
    }
    variables_[name] = ss.str();
}

void TemplateEngine::setCondition(const std::string& name, bool value) {
    conditions_[name] = value;
}

void TemplateEngine::setLoop(const std::string& loop_name, 
                           int item_count,
                           std::function<std::string(const std::string&, int)> handler) {
    loops_[loop_name] = [item_count, handler](const std::string& loop_body) -> std::string {
        std::stringstream result;
        for (int i = 0; i < item_count; ++i) {
            result << handler(loop_body, i);
        }
        return result.str();
    };
}

std::string TemplateEngine::render() const {
    std::string result = template_content_;
    
    // 处理循环语句
    result = processLoops(result);
    
    // 处理条件语句
    result = processConditions(result);
    
    // 替换变量
    result = replaceVariables(result);
    
    return result;
}

std::string TemplateEngine::replaceVariables(const std::string& content) const {
    std::string result = content;
    
    // 使用正则表达式替换变量
    std::regex var_regex("\\{\\{\\s*([a-zA-Z0-9_]+)\\s*\\}\\}");
    std::smatch match;
    
    std::string::const_iterator search_start(result.cbegin());
    std::string temp_result;
    
    while (std::regex_search(search_start, result.cend(), match, var_regex)) {
        temp_result.append(match.prefix());
        
        std::string var_name = match[1].str();
        auto it = variables_.find(var_name);
        
        if (it != variables_.end()) {
            temp_result.append(it->second);
        } else {
            // 保留未知变量
            temp_result.append(match[0].str());
        }
        
        search_start = match.suffix().first;
    }
    
    temp_result.append(search_start, result.cend());
    result = temp_result;
    
    return result;
}

std::string TemplateEngine::processConditions(const std::string& content) const {
    std::string result = content;
    
    // 处理条件语句
    std::regex if_regex("\\{\\%\\s*if\\s+([a-zA-Z0-9_]+)\\s*\\%\\}(.+?)\\{\\%\\s*endif\\s*\\%\\}");
    std::smatch match;
    
    std::string::const_iterator search_start(result.cbegin());
    std::string temp_result;
    
    while (std::regex_search(search_start, result.cend(), match, if_regex, 
                             std::regex_constants::match_default | std::regex_constants::format_default)) {
        temp_result.append(match.prefix());
        
        std::string cond_name = match[1].str();
        std::string cond_body = match[2].str();
        
        auto it = conditions_.find(cond_name);
        if (it != conditions_.end() && it->second) {
            // 条件为真，保留内容
            temp_result.append(cond_body);
        }
        
        search_start = match.suffix().first;
    }
    
    temp_result.append(search_start, result.cend());
    result = temp_result;
    
    // 处理else语句
    std::regex ifelse_regex("\\{\\%\\s*if\\s+([a-zA-Z0-9_]+)\\s*\\%\\}(.+?)\\{\\%\\s*else\\s*\\%\\}(.+?)\\{\\%\\s*endif\\s*\\%\\}");
    
    search_start = result.cbegin();
    temp_result.clear();
    
    while (std::regex_search(search_start, result.cend(), match, ifelse_regex, 
                             std::regex_constants::match_default | std::regex_constants::format_default)) {
        temp_result.append(match.prefix());
        
        std::string cond_name = match[1].str();
        std::string if_body = match[2].str();
        std::string else_body = match[3].str();
        
        auto it = conditions_.find(cond_name);
        if (it != conditions_.end() && it->second) {
            // 条件为真，使用if部分
            temp_result.append(if_body);
        } else {
            // 条件为假，使用else部分
            temp_result.append(else_body);
        }
        
        search_start = match.suffix().first;
    }
    
    temp_result.append(search_start, result.cend());
    result = temp_result;
    
    return result;
}

std::string TemplateEngine::processLoops(const std::string& content) const {
    std::string result = content;
    
    for (const auto& loop_pair : loops_) {
        const std::string& loop_name = loop_pair.first;
        const auto& loop_handler = loop_pair.second;
        
        std::regex loop_start_regex("\\{\\%\\s*for\\s+" + loop_name + "\\s*\\%\\}");
        std::regex loop_end_regex("\\{\\%\\s*endfor\\s*\\%\\}");
        
        size_t start_pos = 0;
        while ((start_pos = result.find("{% for " + loop_name + " %}", start_pos)) != std::string::npos) {
            size_t end_pos = 0;
            std::string loop_body = extractLoopBody(result, loop_name, start_pos, end_pos);
            
            if (end_pos != std::string::npos) {
                // 替换循环
                std::string expanded = loop_handler(loop_body);
                result.replace(start_pos, end_pos - start_pos + 13, expanded);  // 13 = length of "{% endfor %}"
            } else {
                // 没有找到结束标记，跳过
                start_pos += 8 + loop_name.length();
            }
        }
    }
    
    return result;
}

std::string TemplateEngine::extractLoopBody(const std::string& content, 
                                          const std::string& loop_name,
                                          size_t start_pos,
                                          size_t& end_pos) const {
    // 循环开始标记长度
    size_t start_tag_len = 8 + loop_name.length();  // "{% for " + loop_name + " %}"
    
    // 查找循环结束标记
    size_t body_start = start_pos + start_tag_len;
    end_pos = content.find("{% endfor %}", body_start);
    
    if (end_pos != std::string::npos) {
        return content.substr(body_start, end_pos - body_start);
    }
    
    return "";
}

} // namespace generator
} // namespace message
} // namespace next_gen
