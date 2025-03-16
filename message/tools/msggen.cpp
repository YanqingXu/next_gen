#include <iostream>
#include <string>
#include <filesystem>
#include "../generator/generator.h"
#include "../include/types.h"
#include "../../include/utils/logger.h"

namespace fs = std::filesystem;
using namespace next_gen::message::generator;

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -i, --input DIR     Input directory containing Lua message definitions" << std::endl;
    std::cout << "  -o, --output DIR    Output directory for generated C++ files" << std::endl;
    std::cout << "  -t, --template DIR  Template directory" << std::endl;
    std::cout << "  -f, --file FILE     Process only specified Lua file" << std::endl;
    std::cout << "  -v, --verbose       Enable verbose output" << std::endl;
    std::cout << "  -h, --help          Display this help message" << std::endl;
}

int main(int argc, char* argv[]) {
    GeneratorConfig config;
    std::string single_file;
    
    // 默认配置
    config.input_dir = fs::current_path().string() + "/definition";
    config.output_dir = fs::current_path().string() + "/generated";
    config.template_dir = fs::current_path().string() + "/generator/templates";
    
    // 解析命令行参数
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-v" || arg == "--verbose") {
            config.verbose = true;
        } else if (arg == "-i" || arg == "--input") {
            if (i + 1 < argc) {
                config.input_dir = argv[++i];
            } else {
                std::cerr << "Error: Missing input directory" << std::endl;
                return 1;
            }
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) {
                config.output_dir = argv[++i];
            } else {
                std::cerr << "Error: Missing output directory" << std::endl;
                return 1;
            }
        } else if (arg == "-t" || arg == "--template") {
            if (i + 1 < argc) {
                config.template_dir = argv[++i];
            } else {
                std::cerr << "Error: Missing template directory" << std::endl;
                return 1;
            }
        } else if (arg == "-f" || arg == "--file") {
            if (i + 1 < argc) {
                single_file = argv[++i];
            } else {
                std::cerr << "Error: Missing file name" << std::endl;
                return 1;
            }
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }
    
    // 初始化日志系统
    next_gen::Logger::init();
    next_gen::Logger::setLevel(config.verbose ? next_gen::LogLevel::DEBUG : next_gen::LogLevel::INFO);
    
    // 创建消息生成器
    MessageGenerator generator(config);
    
    // 初始化生成器
    if (!generator.initialize()) {
        std::cerr << "Failed to initialize message generator" << std::endl;
        return 1;
    }
    
    // 处理消息定义
    int count = 0;
    if (!single_file.empty()) {
        // 处理单个文件
        std::string lua_file = single_file;
        if (!fs::path(lua_file).is_absolute()) {
            lua_file = fs::path(config.input_dir) / single_file;
        }
        
        if (generator.generateFile(lua_file)) {
            count = 1;
        }
    } else {
        // 处理所有文件
        count = generator.generateAll();
    }
    
    if (count > 0) {
        std::cout << "Successfully generated " << count << " message file(s)" << std::endl;
        return 0;
    } else {
        std::cerr << "No message files were generated" << std::endl;
        return 1;
    }
}
