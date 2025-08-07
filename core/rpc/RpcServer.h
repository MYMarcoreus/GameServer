#pragma once

#include "core_definations.h"
#include "log.h"


namespace yy::net
{
class TcpServer;
}

namespace yy::core::zk
{
class ZkServiceClient;
}


namespace yy::core::rpc
{

// 服务提供方
class RpcServer {
public:
    RpcServer(net::EventLoop* accpetorLoop, const net::IPAddressPtr& listenAddr, const std::string & service_root = "/rpc_services");
    ~RpcServer();

    /// @brief Start Listen & IOLoop
    void Start(int ioThreadNum, net::Milliseconds ioWaitTimeout, const net::F_ThreadInitCallback& cb = nullptr) ;

    /// @brief 结束服务器
    void Stop();

    template <typename ServiceType, typename... Args>
    requires requires(ServiceType t) {
        requires std::derived_from<ServiceType, google::protobuf::Service>;
        requires (!requires {
            { t.channel() } -> std::convertible_to<google::protobuf::RpcChannel*>;
        });
    }
    void RegisterService(Args&&... args);

    template <typename ServiceType>
    requires requires(ServiceType t) {
        requires std::derived_from<ServiceType, google::protobuf::Service>;
        requires (!requires {
            { t.channel() } -> std::convertible_to<google::protobuf::RpcChannel*>;
        });
    }
    void RegisterService(std::unique_ptr<ServiceType> && service);

    auto GetServiceRoot() const -> const std::string& { return service_root_; }

    auto GetZkServiceManager() -> zk::ZkServiceClient& { return *zkServiceManager_; }

private:
    void OnRpcRequest(const net::TcpConnectionPtr& conn, const RpcMessagePtr& msg);
    void SendRpcResponse(net::TcpConnectionPtr conn, std::pair<google::protobuf::Message*, int64_t> pair_response_id);

    ///@return 默认情况下，std::optional<T> 不能保存引用，比如std::optional<google::protobuf::Service&>是不合法❌的
    auto GetService(const std::string& name) const -> std::optional<std::reference_wrapper<google::protobuf::Service>>;


    const std::string service_root_;
    net::EventLoop* loop_;
    std::unique_ptr<net::TcpServer>      server_;
    std::unique_ptr<RpcCodec>            codec_;
    std::unordered_map<std::string, std::unique_ptr<google::protobuf::Service>> services_;
    net::IPAddressPtr   listenAddr_;
    std::unique_ptr<zk::ZkServiceClient> zkServiceManager_;
};

template <typename ServiceType, typename... Args>
requires requires(ServiceType t) {
    // 必须继承自 protobuf::Service
    requires std::derived_from<ServiceType, google::protobuf::Service>;
    requires (!requires {
        { t.channel() } -> std::convertible_to<google::protobuf::RpcChannel*>;
    });
}
void RpcServer::RegisterService(Args&&... args)
{
    auto service = std::make_unique<ServiceType>(std::forward<Args>(args)...);
    const auto* service_desc = service->GetDescriptor();
    const std::string service_name = service_desc->name();

    if (services_.contains(service_name)) {
        YLOG_WARN("[RPC Server] Service {} is already exist", service_name);
        return;
    }

    services_[service_name] = std::move(service);
    // 注册zookeeper服务
    // zkServiceManager_.Register(service_name, listenAddr_->GetIPStr(), listenAddr_->GetPortStr());
    YLOG_INFO("[RPC Server] Registered service {} in Local", service_name);
}

template <typename ServiceType> requires requires (ServiceType t) {
    requires std::derived_from<ServiceType, google::protobuf::Service>;
    requires (!requires { { t.channel() } -> std::convertible_to<google::protobuf::RpcChannel*>; });
}
void RpcServer::RegisterService(std::unique_ptr<ServiceType> && service)
{
    const auto* service_desc = service->GetDescriptor();
    const std::string service_name = service_desc->name();

    if (services_.contains(service_name)) {
        YLOG_WARN("[RPC Server] Service {} is already exist", service_name);
        return;
    }

    services_[service_name] = std::move(service);
    YLOG_INFO("[RPC Server] Registered service {} in Local", service_name);
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
