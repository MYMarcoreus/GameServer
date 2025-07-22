#pragma once

#include "core_definations.h"
#include "IServer.h"
#include "IGameBase.h"
#include "account.pb.h"
#include "AccountRpcClient.h"
#include "ProtobufDispatcher.h"
#include "RpcStubConnectionPool.hpp"
#include "RpcControllerImpl.h"
#include "ThreadPool.h"

using namespace yy::net;
using namespace yy::core;
using std::shared_ptr;

namespace yy::app::gate
{


class GateServerManager final : public Singleton<GateServerManager> {
    SINGLETON_NECESSITY(GateServerManager)
public:
    void RunApp();

    IServer& GetServer() const { return *m_frontend; }
private:
    GateServerManager();
    ~GateServerManager() override;

    void Init();

    void OnFrontend_Secutiry(const UserConnectionPtr& userconn) ;
    void OnFrontend_Disconnect(const UserConnectionPtr& userconn) ;
    void OnFrontend_Message(const UserConnectionPtr &, const MessagePtr &, MessageType type);

    void UnkonwnCommand(const UserConnectionPtr &, const MessagePtr &);

    template<IsProtobufMessage Request, IsProtobufMessage Response>
    void RegisterRpcForward();

    void TestLogin1();
    void TestLogin2();
    void TestLogin3();
    void TestRegister1();


    std::unique_ptr<EventLoop>  m_accpetorLoop;
    std::unique_ptr<IServer> m_frontend;
    ProtobufDispatcher<UserConnectionPtr> m_dispatcher; // 处理下层(core层)分发传来的无法处理的消息
    std::unique_ptr<AccountRpcClient>   m_accountRpcClient;

    ThreadPool m_workThreads;
};




template<IsProtobufMessage Request, IsProtobufMessage Response>
void GateServerManager::RegisterRpcForward()
{
    m_dispatcher.RegisterMessageCallback<Request>(
        [this](const UserConnectionPtr& userconn, const std::shared_ptr<Request>& request)
        {
            // 发起RPC请求
            const bool success = m_accountRpcClient->CallRemoteAsync<Request, Response>(
                request,
                // 设置Rpc响应回调
                [userconn](std::unique_ptr<Response>&& response, std::unique_ptr<rpc::RpcControllerImpl>&& controller)
                {
                    if (!controller || controller->Failed()) {
                        YLOG_INFO("{}失败！", Request::descriptor()->name());
                        return;
                    }

                    if (response) {
                        userconn->SendTCP(*response);
                        YLOG_INFO("{}返回：{}", Response::descriptor()->name(), response->ShortDebugString());
                    }
                });

            // RPC请求是否发送成功
            if (not success) {
                YLOG_WARN("{}转发失败", Request::descriptor()->name());
                userconn->Shutdown();
            }
        });
}
}
