#pragma once

#include "RpcClientPool.hpp"
#include "TcpConnection.h"
#include "EventLoop.h"
#include "log.h"
#include "rpc.pb.h"
#include "RemoteXmlConfig.h"
#include "RpcController.h"
#include "login.pb.h"

using yy::protocol::core::RpcMessage;
using yy::protocol::app::AccountServiceRpc;
using yy::protocol::app::AccountServiceRpc_Stub;
using yy::protocol::app::S2CLogin;
using yy::protocol::app::C2SLogin;

namespace yy::app::gate
{

class AccountRpcClient
{
public:
    explicit AccountRpcClient(yy::net::EventLoop * loop): pool_{10}, loop_{loop}
    {
        pool_.SetConnectionEstablishedCallback([this](const yy::net::TcpConnectionPtr& conn) {
            YLOG_INFO("连接至<{}:{}>，我方地址为<{}:{}>", conn->GetPeerAddr()->GetIPStr().c_str(), conn->GetPeerAddr()->GetPort()
                                                     , conn->GetLocalAddr()->GetIPStr().c_str(), conn->GetLocalAddr()->GetPort());
        });
        pool_.SetServiceChangeCallback([this](std::vector<yy::net::IPAddressPtr>&&) {
            cur_context_ = pool_.Acquire_AutoConnect();
        });

        pool_.Start();
    }

    void Start()
    {
        auto timerid = loop_->RunEvery(10ms, [this]() {
            if (cur_context_ == nullptr) {
                YLOG_ERROR("不存在有效的服务，服务连接池返回空指针")
                return;
            }
            SendLogin();
        });

        loop_->RunAfter(300s, [this, timerid]() {
            this->loop_->CancelTimer(timerid);
        });
    }

    void SendLogin()
    {
        static std::atomic_size_t cnt_ = 0;

        C2SLogin request;
        request.set_account_name("nice_client" + std::to_string(cnt_));
        request.set_password("good_pwd" + std::to_string(cnt_));
        request.set_session_id(cnt_);
        ++cnt_;

        auto response = new S2CLogin;
        auto controller = new yy::core::RpcController;
        controller->set_wait_for_ready(true);
        controller->set_timeout(5s);

        cur_context_->Stub().Login(
            controller,
            &request,
            response,
            yy::core::NewLambdaClosureT([this, response, controller]()  {
                auto resp = std::unique_ptr<S2CLogin>(response);
                auto ctrl = std::unique_ptr<yy::core::RpcController>(controller);
                this->LoginFinished(std::move(resp), std::move(ctrl));
            })
        );
    }

private:
    void LoginFinished(std::unique_ptr<S2CLogin> && response, std::unique_ptr<yy::core::RpcController> && controller)
    {
        if (controller->Failed()) {
            YLOG_INFO("Login失败！");
        } else {
            YLOG_INFO("Login返回结果：{}, {}, {}", response->account_id(), response->account_name(), response->session_id());
        }
    }

    yy::core::RpcClientPool<AccountServiceRpc>          pool_;
    std::shared_ptr<decltype(pool_)::RpcClientContext>  cur_context_;
    yy::net::EventLoop *                                loop_;
};

}
