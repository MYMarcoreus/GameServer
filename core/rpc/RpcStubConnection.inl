#pragma once
template <IsValidStub ServiceType_Stub>
RpcStubConnection<ServiceType_Stub>::RpcStubConnection(net::EventLoop* loop):
    rpc_conn_{new RpcConnection(loop)},
    stub_{std::make_unique<ServiceType_Stub>(rpc_conn_, google::protobuf::Service::STUB_OWNS_CHANNEL)}
{
    assert(rpc_conn_ != nullptr);
    assert(stub_ != nullptr);
}

template <IsValidStub ServiceType_Stub>
std::string RpcStubConnection<ServiceType_Stub>::GetServerName() const
{ return std::format("{}:{}", server_addr_->GetIPStr(), server_addr_->GetPort()); }

template <IsValidStub ServiceType_Stub>
bool RpcStubConnection<ServiceType_Stub>::Connect(const net::IPAddressPtr& server_addr)
{
    const bool isconn = rpc_conn_->Connect(server_addr);
    server_addr_ = server_addr;

    return isconn;
}

template <IsValidStub ServiceType_Stub>
void RpcStubConnection<ServiceType_Stub>::Disconnect()
{
    rpc_conn_->Disconnect();
}

template <IsValidStub ServiceType_Stub>
void RpcStubConnection<ServiceType_Stub>::SetConnectionEstablishedCallback(
    const F_RpcStubConnectionEstablishedCallback& connectionEstablishedCallback) const
{
    rpc_conn_->SetConnectionEstablishedCallback(connectionEstablishedCallback);
}

template <IsValidStub ServiceType_Stub>
std::string RpcStubConnection<ServiceType_Stub>::GetServiceName()
{
    return ServiceType_Stub::descriptor()->name();
}
