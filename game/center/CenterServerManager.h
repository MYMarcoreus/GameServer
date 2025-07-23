#pragma once

#include <memory>
#include "Singleton.h"

namespace yy::core::rpc { class RpcServer; }
namespace yy::net { class EventLoop; }

using namespace yy::net;
using namespace yy::core;
using std::shared_ptr;


namespace yy::app::center
{
class CenterServerManager final : public Singleton<CenterServerManager> {
    SINGLETON_NECESSITY(CenterServerManager)

public:
    void RunApp();

    rpc::RpcServer& GetRpcServer() const { return *m_rpcServer; }
private:
    CenterServerManager();
    ~CenterServerManager() override;

    std::unique_ptr<rpc::RpcServer> m_rpcServer;
    std::unique_ptr<EventLoop> m_accpetorLoop;
};

}
