#ifndef NEXT_GEN_CONFIG_H
#define NEXT_GEN_CONFIG_H

#include <cstdint>
#include <string>

// Version information
#define NEXT_GEN_VERSION_MAJOR 0
#define NEXT_GEN_VERSION_MINOR 1
#define NEXT_GEN_VERSION_PATCH 0

// Platform detection
#if defined(_WIN32) || defined(_WIN64)
    #define NEXT_GEN_PLATFORM_WINDOWS
#elif defined(__linux__)
    #define NEXT_GEN_PLATFORM_LINUX
#elif defined(__APPLE__)
    #define NEXT_GEN_PLATFORM_MACOS
#else
    #error "Unsupported platform"
#endif

// Export macro definition
#ifdef NEXT_GEN_PLATFORM_WINDOWS
    #ifdef NEXT_GEN_EXPORTS
        #define NEXT_GEN_API __declspec(dllexport)
    #else
        #define NEXT_GEN_API __declspec(dllimport)
    #endif
#else
    #define NEXT_GEN_API __attribute__((visibility("default")))
#endif

// Namespace
namespace next_gen {

// Basic type definitions
using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using f32 = float;
using f64 = double;

// Server configuration
struct ServerConfig {
    std::string server_name;
    std::string ip;
    u16 port;
    u32 max_connections;
    u32 thread_pool_size;
    u32 message_queue_size;
    bool enable_monitoring;
    
    // Default configuration
    ServerConfig() :
        server_name("NextGenServer"),
        ip("0.0.0.0"),
        port(8888),
        max_connections(10000),
        thread_pool_size(4),
        message_queue_size(10000),
        enable_monitoring(true)
    {}
};

} // namespace next_gen

#endif // NEXT_GEN_CONFIG_H
