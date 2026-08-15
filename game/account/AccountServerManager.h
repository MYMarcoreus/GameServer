#pragma once

#include "Singleton.h"
#include "ProtobufDispatcher.h"

namespace yy::net{ class ThreadPool; }
namespace yy::core{ class IServer; }
namespace yy::core::rpc{ class RpcServer;}

namespace yy::app::account
{

class AccountServerManager final : public Singleton<AccountServerManager> {
    SINGLETON_NECESSITY(AccountServerManager)
public:
    void RunApp();

private:
    AccountServerManager();

    ~AccountServerManager() override;

private:
    std::unique_ptr<core::rpc::RpcServer> m_rpcServer;
    std::unique_ptr<net::EventLoop> m_accpetorLoop;
};

}
