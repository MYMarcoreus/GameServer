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


namespace yy::app::center
{
class CenterServerManager final : public Singleton<CenterServerManager> {
    SINGLETON_NECESSITY(CenterServerManager)

public:
    void RunApp();

    core::rpc::RpcServer& GetRpcServer() const { return *m_rpcServer; }
private:
    CenterServerManager();
    ~CenterServerManager() override;

    std::unique_ptr<core::rpc::RpcServer> m_rpcServer;
    std::unique_ptr<net::EventLoop> m_accpetorLoop;
};

}
