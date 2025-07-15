#pragma once

#include "IServer.h"
#include "IGameBase.h"
#include "ProtobufDispatcher.h"
#include "ThreadPool.h"

namespace yy::core::rpc
{
class RpcServer;
}

using yy::core::IServer;
using std::shared_ptr;


namespace yy::app::account
{

class AccountServerManager final : public Singleton<AccountServerManager> {
    SINGLETON_NECESSITY(AccountServerManager)
public:
    void RunApp();

    IServer&            GetServer() const { return *m_server; }
    core::rpc::RpcServer&    GetRpcServer() const { return *m_rpcServer; }
private:
    AccountServerManager();
    ~AccountServerManager() override;

    void Init();

    void AppNotifier_Secutiry(const core::UserConnectionPtr& userdata) ;
    void AppNotifier_Disconnect(const core::UserConnectionPtr& userdata) ;
    void AppNotifier_Command(const core::UserConnectionPtr &, const core::MessagePtr &, const core::MessageType);

    void UnkonwnCommand(const core::UserConnectionPtr &, const core::MessagePtr &);


    std::unique_ptr<IServer> m_server;
    std::unique_ptr<core::rpc::RpcServer> m_rpcServer;
    core::ProtobufDispatcher<core::UserConnectionPtr> m_dispatcher; // 处理下层(core层)分发传来的无法处理的消息
    std::unique_ptr<yy::net::EventLoop> m_accpetorLoop;


    yy::net::ThreadPool m_wordThreads;
};

}
