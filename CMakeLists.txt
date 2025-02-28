cmake_minimum_required(VERSION 3.10)
project(NextGen VERSION 1.0.0 LANGUAGES CXX)

# C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 查找 ASIO 库
find_package(asio CONFIG REQUIRED)

# 包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${asio_INCLUDE_DIRS}
)

# 源文件
set(NEXT_GEN_SOURCES
    "src/network/net_service.cpp"
    "src/network/tcp_service.cpp"
    "src/network/tcp_session.cpp"
)

# 头文件
set(NEXT_GEN_HEADERS
    "include/core/config.h"
    "include/core/service.h"
    "include/message/message.h"
    "include/message/message_queue.h"
    "include/module/module.h"
    "include/module/module_impl.h"
    "include/module/module_interface.h"
    "include/network/asio_wrapper.h"
    "include/network/net_service.h"
    "include/network/tcp_service.h"
    "include/network/tcp_session.h"
    "include/utils/error.h"
    "include/utils/logger.h"
    "include/utils/timer.h"
)

# 示例源文件
file(GLOB EXAMPLE_SOURCES "examples/*.cpp")

# 定义库
add_library(next_gen SHARED ${NEXT_GEN_SOURCES} ${NEXT_GEN_HEADERS})

# 库定义
target_compile_definitions(next_gen PRIVATE NEXT_GEN_EXPORTS)
target_link_libraries(next_gen ${asio_LIBRARIES})

# 示例应用程序
foreach(EXAMPLE_SOURCE ${EXAMPLE_SOURCES})
    get_filename_component(EXAMPLE_NAME ${EXAMPLE_SOURCE} NAME_WE)
    add_executable(${EXAMPLE_NAME} ${EXAMPLE_SOURCE})
    target_link_libraries(${EXAMPLE_NAME} next_gen)
endforeach()

# Windows 特定设置
if(WIN32)
    target_link_libraries(next_gen ws2_32)
endif()

# 安装规则
install(TARGETS next_gen
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/ DESTINATION include/next_gen)
