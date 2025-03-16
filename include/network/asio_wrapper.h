#ifndef NEXT_GEN_ASIO_WRAPPER_H
#define NEXT_GEN_ASIO_WRAPPER_H

// 定义 ASIO_STANDALONE 以便使用独立版本的 ASIO（不依赖 Boost）
#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif

// 包含 ASIO 头文件
#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
using namespace boost;

namespace next_gen {

// Common ASIO type aliases to simplify usage
using AsioService = asio::io_service;
using AsioServiceWork = asio::io_service::work;
using AsioErrorCode = boost::system::error_code;

// TCP related types
using AsioTcpSocket = asio::ip::tcp::socket;
using AsioTcpAcceptor = asio::ip::tcp::acceptor;
using AsioTcpEndpoint = asio::ip::tcp::endpoint;
using AsioTcpResolver = asio::ip::tcp::resolver;

// UDP related types
using AsioUdpSocket = asio::ip::udp::socket;
using AsioUdpEndpoint = asio::ip::udp::endpoint;
using AsioUdpResolver = asio::ip::udp::resolver;

// Common buffer types
using AsioBuffer = asio::mutable_buffer;
using AsioConstBuffer = asio::const_buffer;

} // namespace next_gen

#endif // NEXT_GEN_ASIO_WRAPPER_H
