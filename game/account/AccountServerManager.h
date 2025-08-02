#pragma once

#include "Singleton.h"
#include "ProtobufDispatcher.h"

namespace yy::net{ class ThreadPool; }
namespace yy::core{ class IServer; }
namespace yy::core::rpc{ class RpcServer;}

using namespace yy::net;
using namespace yy::core;
using std::shared_ptr;


namespace yy::app::account
{

class AccountServerManager final : public Singleton<AccountServerManager> {
    SINGLETON_NECESSITY(AccountServerManager)
public:
    void RunApp();

    IServer&            GetServer() const { return *m_server; }
    rpc::RpcServer&     GetRpcServer() const { return *m_rpcServer; }
private:
    AccountServerManager();
    ~AccountServerManager() override;

    void Init();

    void AppNotifier_Secutiry(const UserConnectionPtr& userdata) ;
    void AppNotifier_Disconnect(const UserConnectionPtr& userdata) ;
    void AppNotifier_Command(const UserConnectionPtr &, const MessagePtr &, const MessageNetType);

    void UnkonwnCommand(const UserConnectionPtr &, const MessagePtr &);


    std::unique_ptr<IServer> m_server;
    std::unique_ptr<rpc::RpcServer> m_rpcServer;
    ProtobufDispatcher<UserConnectionPtr> m_dispatcher; // 处理下层(core层)分发传来的无法处理的消息
    std::unique_ptr<EventLoop> m_accpetorLoop;
    std::unique_ptr<ThreadPool> m_wordThreads;
};

}
