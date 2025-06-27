#pragma once

#include "RpcCodec.h"
#include <map>

#include "TcpServer.h"

namespace google::protobuf
{
class Descriptor;            // descriptor.h
class ServiceDescriptor;     // descriptor.h
class MethodDescriptor;      // descriptor.h
class Message;               // message.h

class Closure;

class RpcController;
class Service;
}

namespace yy::core
{

class RpcServer {
public:
    RpcServer(yy::net::EventLoop* accpetorLoop, const yy::net::IPAddressPtr& listenAddr);
    ~RpcServer();

    /// @brief Start Listen & IOLoop
    void Start() ;

    /// @brief 结束服务器
    void Stop() ;

    void RegisterService(google::protobuf::Service *);

private:
    void OnRpcRequest(const net::TcpConnectionPtr& conn, const RpcMessagePtr& msg);
    void SendRpcResponse(const net::TcpConnectionPtr& conn, const std::pair<google::protobuf::Message* , int64_t>& response_id);

    yy::net::EventLoop* loop_;
    net::TcpServer server_;

    std::map<std::string, ::google::protobuf::Service*> services_;
    RpcCodec codec_;
};

}
