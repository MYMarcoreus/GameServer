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
using yy::protocol::app::S2CRegister;
using yy::protocol::app::C2SRegister;
using yy::protocol::app::S2CLogin;
using yy::protocol::app::C2SLogin;

namespace yy::app::gate
{

class AccountRpcClient
{
public:
    explicit AccountRpcClient():
        pool_{10}
    {
        pool_.SetConnectionEstablishedCallback([this](const yy::net::TcpConnectionPtr& conn) {
            YLOG_INFO("连接至<{}:{}>，我方地址为<{}:{}>", conn->GetPeerAddr()->GetIPStr().c_str(), conn->GetPeerAddr()->GetPort()
                                                     , conn->GetLocalAddr()->GetIPStr().c_str(), conn->GetLocalAddr()->GetPort());
        });
        pool_.SetServiceChangeCallback([this](const std::string & path, std::vector<yy::net::IPAddressPtr>&&) {
            YLOG_INFO("ServiceChange to {}", path)
            cur_context_ = pool_.Acquire_AutoConnect();
        });

        pool_.Start();
    }

    bool ForwardLogin(const std::shared_ptr<C2SLogin>& request)
    {
        if (cur_context_) return false;

        auto response = new S2CLogin;
        auto controller = new yy::core::RpcController;
        controller->set_wait_for_ready(true);
        controller->set_timeout(5s);

        cur_context_->Stub().Login(
            controller,
            request.get(),
            response,
            yy::core::NewLambdaClosureT([this, response, controller]()  {
                auto resp = std::unique_ptr<S2CLogin>(response);
                auto ctrl = std::unique_ptr<yy::core::RpcController>(controller);
                this->LoginFinished(std::move(resp), std::move(ctrl));
            })
        );

        return true;
    }

    bool ForwardRegister(const std::shared_ptr<C2SRegister>& request)
    {
        if (cur_context_) return false;

        auto response = new S2CRegister;
        auto controller = new yy::core::RpcController;
        controller->set_wait_for_ready(true);
        controller->set_timeout(5s);

        cur_context_->Stub().Register(
            controller,
            request.get(),
            response,
            yy::core::NewLambdaClosureT([this, response, controller]()  {
                auto resp = std::unique_ptr<S2CRegister>(response);
                auto ctrl = std::unique_ptr<yy::core::RpcController>(controller);
                this->RegisterFinished(std::move(resp), std::move(ctrl));
            })
        );

        return true;
    }

private:
    void LoginFinished(std::unique_ptr<S2CLogin> && response, std::unique_ptr<yy::core::RpcController> && controller)
    {
        if (controller->Failed()) {
            YLOG_INFO("Login失败！");
        } else {
            YLOG_INFO("Login返回结果：{}, {}, {}", response->session_id(), response->account_id(), response->account_name());
        }
    }

    void RegisterFinished(std::unique_ptr<S2CRegister> && response, std::unique_ptr<yy::core::RpcController> && controller)
    {
        if (controller->Failed()) {
            YLOG_INFO("Register失败！");
        } else {
            YLOG_INFO("Register返回结果：{}, {}", response->session_id(), static_cast<int>(response->result_code()));
        }
    }

    yy::core::RpcClientPool<AccountServiceRpc>          pool_;
    std::shared_ptr<decltype(pool_)::RpcClientContext>  cur_context_;
};

}
