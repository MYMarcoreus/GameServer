#pragma once

#include "RWLock.h"
#include "TcpConnection.h"
#include "RpcConnection.h"
#include <functional>


namespace yy::core::rpc
{
template <typename T>
concept IsValidStub =
    std::derived_from<T, google::protobuf::Service> &&
    requires(T t) {
    { t.channel() } -> std::convertible_to<google::protobuf::RpcChannel*>;
    };

template <IsValidStub ServiceType_Stub>
class RpcStubConnection
{
public:
    // using F_RpcStubConnectionEstablishedCallback = std::function<void(const std::shared_ptr<RpcStubConnection>& )>;
    using F_RpcStubConnectionEstablishedCallback = std::function<void(const net::TcpConnectionPtr & )>;
    explicit RpcStubConnection(net::EventLoop * loop);

    net::IPAddressPtr GetServerAddr() const { return server_addr_; }
    std::string GetServerName() const;

    bool Connect(const net::IPAddressPtr& server_addr);

    void Disconnect();

    ServiceType_Stub& Stub()
    {
        return *stub_;
    }

    void SetConnectionEstablishedCallback(const F_RpcStubConnectionEstablishedCallback &connectionEstablishedCallback) const;

    static std::string GetServiceName();

private:
    RpcConnection *                     rpc_conn_;
    std::unique_ptr<ServiceType_Stub>   stub_;
    net::IPAddressPtr server_addr_;
};

#include "RpcStubConnection.inl"

}
