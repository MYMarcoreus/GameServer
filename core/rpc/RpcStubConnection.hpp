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
    explicit RpcStubConnection(net::EventLoop * loop):
        rpc_conn_{new RpcConnection(loop)},
        stub_{std::make_unique<ServiceType_Stub>(rpc_conn_, google::protobuf::Service::STUB_OWNS_CHANNEL)}
    {
        assert(rpc_conn_ != nullptr);
        assert(stub_ != nullptr);
    }

    net::IPAddressPtr GetServerAddr() const { return server_addr_; }
    std::string GetServerName() const { return util::GenerateServerName(server_addr_); }

    bool Connect(const net::IPAddressPtr& server_addr)
    {
        const bool isconn = rpc_conn_->Connect(server_addr);
        server_addr_ = server_addr;

        return isconn;
    }

    void Disconnect()
    {
        rpc_conn_->Disconnect();
    }

    ServiceType_Stub& Stub()
    {
        return *stub_;
    }

    void SetConnectionEstablishedCallback(const F_RpcStubConnectionEstablishedCallback &connectionEstablishedCallback) const
    {
        rpc_conn_->SetConnectionEstablishedCallback(connectionEstablishedCallback);
    }

    static std::string GetServiceName()
    {
        return ServiceType_Stub::descriptor()->name();
    }

private:
    RpcConnection *                     rpc_conn_;
    std::unique_ptr<ServiceType_Stub>   stub_;
    net::IPAddressPtr server_addr_;
};

}