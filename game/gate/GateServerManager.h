#pragma once


#include "IServer.h"
#include "IGameBase.h"
#include "account.pb.h"
#include "AccountRpcClient.h"
#include "ProtobufDispatcher.h"
#include "RpcStubPool.hpp"
#include "RpcControllerImpl.h"
#include "ThreadPool.h"

using yy::core::IServer;
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

    void OnFrontend_Secutiry(const core::UserConnectionPtr& userconn) ;
    void OnFrontend_Disconnect(const core::UserConnectionPtr& userconn) ;
    void OnFrontend_Message(const core::UserConnectionPtr &, const core::MessagePtr &, core::MessageType type);

    void UnkonwnCommand(const core::UserConnectionPtr &, const core::MessagePtr &);

    template<typename Request, typename Response>  requires requires {
        requires std::is_base_of_v<google::protobuf::Message, Request>;
        requires std::is_base_of_v<google::protobuf::Message, Response>;
    }
    void RegisterRpcForward();

    void TestRpcConnectionEstablished(const yy::net::TcpConnectionPtr& conn);


    std::unique_ptr<yy::net::EventLoop>  m_accpetorLoop;
    std::unique_ptr<IServer> m_frontend;
    core::ProtobufDispatcher<core::UserConnectionPtr> m_dispatcher; // 处理下层(core层)分发传来的无法处理的消息
    std::unique_ptr<AccountRpcClient>   m_accountRpcClient;

    yy::net::ThreadPool m_workThreads;
};




template<typename Request, typename Response>  requires requires {
    requires std::is_base_of_v<google::protobuf::Message, Request>;
    requires std::is_base_of_v<google::protobuf::Message, Response>;
}
void GateServerManager::RegisterRpcForward()
{
    m_dispatcher.RegisterMessageCallback<Request>(
        [this](const core::UserConnectionPtr& user, const std::shared_ptr<Request>& request)
        {
            const bool success = m_accountRpcClient->CallRemoteAsync<Request, Response>(
                request,
                [user](std::unique_ptr<Response>&& response, std::unique_ptr<yy::core::RpcControllerImpl>&& controller)
                {
                    if (!controller || controller->Failed()) {
                        YLOG_INFO("{}失败！", Request::descriptor()->name());
                        return;
                    }

                    if (response) {
                        user->SendTCP(*response);
                        YLOG_INFO("{}返回：{}", Response::descriptor()->name(), response->ShortDebugString());
                    }
                });

            if (!success) {
                YLOG_WARN("{}转发失败", Request::descriptor()->name());
                user->Shutdown();
            }
        });
}
}
