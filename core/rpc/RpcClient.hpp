#pragma once
#include "log.h"
#include "RpcControllerImpl.h"
#include "RpcStubConnectionPool.hpp"
#include "Singleton.h"

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
    using StubConnType = typename StubConnPoolType::StubConnType;

    static std::string GetServiceName()
    {
        return RpcStubConnectionPool<ServiceStub>::GetServiceName();
    }

    void SetConnectionEstablishedCallback(typename StubConnType::F_RpcStubConnectionEstablishedCallback cb)
    {
        cb_ = std::move(cb);
    }

    void Start(const size_t pool_size)
    {
        if (pool_ == nullptr) {
            pool_ = std::make_unique<RpcStubConnectionPool<ServiceStub>>(pool_size);
            pool_->SetServiceChangeCallback(
                [this](const std::string & service_base, std::unordered_map<std::string, net::IPAddressPtr> providers) {
                    YLOG_INFO("ServiceChange to {}", service_base)
                });
            pool_->Start(std::move(cb_));
        }
    }

    void Start()
    {
        Start(0);
    }

    auto GetServerNames() -> std::unordered_map<std::string, net::IPAddressPtr>
    {
        Start();
        return pool_->GetServerNames();
    }

    ///@brief 请求的发送的同步的，响应的等待是异步的
    /// Request消息的生命周期由调用者自己管理
    /// Respone消息的生命周期由该函数自动管理
    template<IsProtobufMessage Request, IsProtobufMessage Response>
    bool CallRemoteAsync_Random(const std::shared_ptr<Request>& request, FinishedCallback<Response> cb)
    {
        Start();

        auto conn = pool_->Acquire_Random(5s);
        if (conn == nullptr) return false;
        if (request == nullptr) return false;

        auto response = new Response;
        // 设置请求参数
        auto controller = new RpcControllerImpl;
        controller->set_wait_for_ready(true);
        controller->set_timeout(5s);

        // 设置响应回调，并使用unique_ptr接管裸指针（响应消息和RpcController的生命周期在此自动管理）
        auto lambda_closure = rpc::NewLambdaClosureT(
            [this, response, controller, cb = std::move(cb)]() mutable  {
                if (cb) cb(std::unique_ptr<Response>(response), std::unique_ptr<RpcControllerImpl>(controller));
            });

        // 通过特化模板函数DoCall调用客户端的`RpcConnection::CallMethod`来同步发送请求
        DoCall<Request, Response>(conn->Stub(), controller, request.get(), response, lambda_closure);

        return true;
    }

    template<IsProtobufMessage Request, IsProtobufMessage Response>
    bool CallRemoteAsync_Random(const Request& request, FinishedCallback<Response> cb)
    {
        Start();

        auto conn = pool_->Acquire_Random(5s);
        if (conn == nullptr) return false;

        auto response = new Response;
        auto controller = new RpcControllerImpl;
        controller->set_wait_for_ready(true);
        controller->set_timeout(5s);

        auto lambda_closure = rpc::NewLambdaClosureT(
            [this, response, controller, cb = std::move(cb)]() mutable {
                if (cb) cb(std::unique_ptr<Response>(response), std::unique_ptr<RpcControllerImpl>(controller));
            });

        DoCall<Request, Response>(conn->Stub(), controller, &request, response, lambda_closure);

        return true;
    }

    ///@brief 请求的发送的同步的，响应的等待是异步的
    /// Request消息的生命周期由调用者自己管理
    /// Respone消息的生命周期由该函数自动管理
    template<IsProtobufMessage Request, IsProtobufMessage Response>
    bool CallRemoteAsync_From(std::string server_name, const std::shared_ptr<Request>& request, FinishedCallback<Response> cb)
    {
        Start();

        auto conn = pool_->Acquire_From(server_name, 5s);
        if (conn == nullptr) return false;
        if (request == nullptr) return false;

        auto response = new Response;
        // 设置请求参数
        auto controller = new RpcControllerImpl;
        controller->set_wait_for_ready(true);
        controller->set_timeout(5s);

        // 设置响应回调，并使用unique_ptr接管裸指针（响应消息和RpcController的生命周期在此自动管理）
        auto lambda_closure = rpc::NewLambdaClosureT(
            [this, response, controller, cb = std::move(cb)]() mutable  {
                if (cb) cb(std::unique_ptr<Response>(response), std::unique_ptr<RpcControllerImpl>(controller));
            });

        // 通过特化模板函数DoCall调用客户端的`RpcConnection::CallMethod`来同步发送请求
        DoCall<Request, Response>(conn->Stub(), controller, request.get(), response, lambda_closure);

        return true;
    }

    template<IsProtobufMessage Request, IsProtobufMessage Response>
    bool CallRemoteAsync_From(const std::string & server_name, const Request& request, FinishedCallback<Response> cb)
    {
        Start();

        auto conn = pool_->Acquire_From(server_name, 5s);
        if (conn == nullptr) return false;

        auto response = new Response;
        auto controller = new RpcControllerImpl;
        controller->set_wait_for_ready(true);
        controller->set_timeout(5s);

        auto lambda_closure = rpc::NewLambdaClosureT(
            [this, response, controller, cb = std::move(cb)]() mutable {
                if (cb) cb(std::unique_ptr<Response>(response), std::unique_ptr<RpcControllerImpl>(controller));
            });

        DoCall<Request, Response>(conn->Stub(), controller, &request, response, lambda_closure);

        return true;
    }



private:
    ///@brief 调用具体客户端ServiceStub的对应方法，若有新的服务和新的方法，仅需在别的源文件中实现模板特化，调用stub->MethodName即可（如stub->Login或stub->Register等）
    template<typename Request, typename Response>
    void DoCall(ServiceStub& stub, RpcControllerImpl* controller,
            const Request* request, Response* response, google::protobuf::Closure* done);

    std::unique_ptr<RpcStubConnectionPool<ServiceStub>>  pool_ = nullptr;
    typename StubConnType::F_RpcStubConnectionEstablishedCallback cb_;
};






}
