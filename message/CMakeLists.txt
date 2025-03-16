cmake_minimum_required(VERSION 3.12)

# 消息系统CMake配置
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# 查找Lua库
find_package(Lua REQUIRED)
include_directories(${LUA_INCLUDE_DIR})

# 消息系统核心库
set(MESSAGE_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/message_base.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/message_factory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/legacy/adapter.h
)

# 消息生成器库
set(MESSAGE_GENERATOR_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/generator/generator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/generator/generator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/generator/generator_impl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/generator/template_engine.h
    ${CMAKE_CURRENT_SOURCE_DIR}/generator/template_engine.cpp
)

# 消息工具源文件
set(MESSAGE_TOOLS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/msggen.cpp
)

# 创建消息核心库
add_library(message_core STATIC ${MESSAGE_CORE_SOURCES})
target_link_libraries(message_core PRIVATE utils)

# 创建消息生成器库
add_library(message_generator STATIC ${MESSAGE_GENERATOR_SOURCES})
target_link_libraries(message_generator PRIVATE message_core utils ${LUA_LIBRARIES})

# 创建消息生成工具
add_executable(msggen ${MESSAGE_TOOLS_SOURCES})
target_link_libraries(msggen PRIVATE message_generator message_core utils ${LUA_LIBRARIES})

# 创建目标目录
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/generated)

# 定义生成消息的自定义命令
add_custom_target(generate_messages
    COMMAND msggen -i ${CMAKE_CURRENT_SOURCE_DIR}/definition -o ${CMAKE_CURRENT_SOURCE_DIR}/generated -t ${CMAKE_CURRENT_SOURCE_DIR}/generator/templates
    DEPENDS msggen
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating message classes from Lua definitions"
)

# 消息库（包含生成的消息类）
file(GLOB_RECURSE GENERATED_MESSAGE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/generated/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/generated/*.cpp")
add_library(messages STATIC ${GENERATED_MESSAGE_SOURCES})
target_link_libraries(messages PRIVATE message_core utils)
add_dependencies(messages generate_messages)

# 安装目标
install(TARGETS message_core message_generator messages msggen
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# 安装头文件
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../include/
    DESTINATION include/next_gen/message
    FILES_MATCHING PATTERN "*.h"
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/generated/
    DESTINATION include/next_gen/message/generated
    FILES_MATCHING PATTERN "*.h"
)
