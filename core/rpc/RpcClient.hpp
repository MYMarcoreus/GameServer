#pragma once
#include "log.h"
#include "RpcControllerImpl.h"
#include "RpcStubConnectionPool.hpp"
#include "Singleton.h"
#include <google/protobuf/stubs/callback.h>

namespace yy::core::rpc
{
///@brief 懒汉模式
template<IsValidStub ServiceStub>
class RpcClient final : public Singleton<RpcClient<ServiceStub>>
{
    SINGLETON_NECESSITY(RpcClient<ServiceStub>)
public:
    template <typename Response>
    using FinishedCallback = std::function<void(std::unique_ptr<Response> && response, std::unique_ptr<RpcControllerImpl> && controller)>;
    using StubConnPoolType = RpcStubConnectionPool<ServiceStub>;
    using StubConnType = StubConnPoolType::StubConnType;

    static std::string GetServiceName();

    void SetConnectionEstablishedCallback(StubConnType::F_RpcStubConnectionEstablishedCallback cb);

    void Start(size_t pool_size);

    void Start()
    {
        Start(0);
    }

    auto GetServerNames() -> std::unordered_map<std::string, net::IPAddressPtr>;

    ///@brief 请求的发送的同步的，响应的等待是异步的
    /// Request消息的生命周期由调用者自己管理
    /// Respone消息的生命周期由该函数自动管理
    template<IsProtobufMessage Request, IsProtobufMessage Response>
    bool CallRemoteAsync_Random(const std::shared_ptr<Request>& request, FinishedCallback<Response> cb);

    template<IsProtobufMessage Request, IsProtobufMessage Response>
    bool CallRemoteAsync_Random(const Request& request, FinishedCallback<Response> cb);

    ///@brief 请求的发送的同步的，响应的等待是异步的
    /// Request消息的生命周期由调用者自己管理
    /// Respone消息的生命周期由该函数自动管理
    template<IsProtobufMessage Request, IsProtobufMessage Response>
    bool CallRemoteAsync_From(std::string server_name, const std::shared_ptr<Request>& request, FinishedCallback<Response> cb);

    template<IsProtobufMessage Request, IsProtobufMessage Response>
    bool CallRemoteAsync_From(const std::string & server_name, const Request& request, FinishedCallback<Response> cb);

private:
    ///@brief 调用具体客户端ServiceStub的对应方法，若有新的服务和新的方法，仅需在别的源文件中实现模板特化，调用stub->MethodName即可（如stub->Login或stub->Register等）
    template<typename Request, typename Response>
    void DoCall(ServiceStub& stub, RpcControllerImpl* controller,
            const Request* request, Response* response, google::protobuf::Closure* done);

    std::unique_ptr<RpcStubConnectionPool<ServiceStub>>  pool_ = nullptr;
    StubConnType::F_RpcStubConnectionEstablishedCallback cb_;
};


#include "RpcClient.inl"
}
