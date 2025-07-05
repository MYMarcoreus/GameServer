#pragma once

#include "RpcConnection.h"
#include "log.h"
#include "RpcCodec.h"
#include "TcpServer.h"
#include "ZkClient.h"

#include <google/protobuf/message.h>
#include <google/protobuf/service.h>



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

// 服务提供方
class RpcServer {
    inline static const std::string kServiceRoot = "/services";
public:
    RpcServer(yy::net::EventLoop* accpetorLoop, const yy::net::IPAddressPtr& listenAddr);
    ~RpcServer();

    /// @brief Start Listen & IOLoop
    void Start() ;

    /// @brief 结束服务器
    void Stop() ;

    template <typename ServiceType>
        requires std::is_base_of_v<google::protobuf::Service, ServiceType>
    void RegisterService();

private:
    void OnRpcRequest(const net::TcpConnectionPtr& conn, const RpcMessagePtr& msg);
    void SendRpcResponse(const net::TcpConnectionPtr& conn, const std::pair<google::protobuf::Message* , int64_t>& pair_response_id);

    ///@return 默认情况下，std::optional<T> 不能保存引用，比如std::optional<google::protobuf::Service&>是不合法❌的
    auto GetService(const std::string& name) const -> std::optional<std::reference_wrapper<google::protobuf::Service>>;


    yy::net::EventLoop* loop_;
    net::TcpServer      server_;
    RpcCodec            codec_;
    std::unordered_map<std::string, std::unique_ptr<google::protobuf::Service>> services_;
    net::IPAddressPtr   listenAddr_;
};

template <typename ServiceType>
    requires std::is_base_of_v<google::protobuf::Service, ServiceType>
void RpcServer::RegisterService()
{
    auto service = std::make_unique<ServiceType>();
    const auto* service_desc = service->GetDescriptor();
    const std::string service_name = service_desc->name();

    if (services_.contains(service_name)) {
        YLOG_WARN("RPC Server: Service {} is already exist", service_name);
    } else {
        services_[service_name] = std::move(service);
        YLOG_INFO("RPC Server: Registered service {}", service_name);
    }
}

inline auto RpcServer::GetService(const std::string& name) const -> std::optional<std::reference_wrapper<google::protobuf::Service>>
{
    const auto it = services_.find(name);
    if (it != services_.end() && it->second) {
        return std::ref(*it->second);
    }
    return std::nullopt;
}

}
