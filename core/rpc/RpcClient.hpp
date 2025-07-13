#pragma once
#include "log.h"
#include "RpcControllerImpl.h"
#include "RpcStubPool.hpp"

#include "account.pb.h"


namespace yy::core
{





template<IsValidStub ServiceStub>
class RpcClient
{
public:
    template <typename Response>
    using FinishedCallback = std::function<void(std::unique_ptr<Response> && response, std::unique_ptr<RpcControllerImpl> && controller)>;

    explicit RpcClient(): pool_{nullptr}, stub_conn_{nullptr}
    {  }

    void Start(size_t pool_size, net::F_ConnectionEstablishedCallback cb)
    {
        if (pool_ == nullptr) {
            pool_ = std::make_unique<RpcStubPool<ServiceStub>>(pool_size, std::move(cb));

            pool_->SetServiceChangeCallback(
                [this](const std::string & path, std::vector<yy::net::IPAddressPtr>&&) {
                    YLOG_INFO("ServiceChange to {}", path)
                    stub_conn_ = pool_->Acquire();
                });

            pool_->Start();
        }
    }

    template<typename Request, typename Response>  requires requires {
        requires std::is_base_of_v<google::protobuf::Message, Request>;
        requires std::is_base_of_v<google::protobuf::Message, Response>;
    }
    bool CallRemoteAsync(const std::shared_ptr<Request>& request, FinishedCallback<Response> cb)
    {
        if (stub_conn_ == nullptr) return false;

        auto response = new Response;
        auto controller = new RpcControllerImpl;
        controller->set_wait_for_ready(true);
        controller->set_timeout(5s);

        auto lambda_closure = core::NewLambdaClosureT(
            [this, response, controller, cb = std::move(cb)]() mutable  {
                auto resp = std::unique_ptr<Response>(response);
                auto ctrl = std::unique_ptr<RpcControllerImpl>(controller);
                cb(std::move(resp), std::move(ctrl));
            });

        DoCall<Request, Response>(stub_conn_->Stub(), controller, request.get(), response, lambda_closure);

        return true;
    }

private:

    template<typename Request, typename Response>
    void DoCall(ServiceStub& stub, RpcControllerImpl* controller,
            Request* request, Response* response, google::protobuf::Closure* done);


    std::unique_ptr<RpcStubPool<ServiceStub>>           pool_;
    std::shared_ptr<RpcStubConnection<ServiceStub>>     stub_conn_;
};






}
