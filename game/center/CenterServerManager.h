#pragma once

#include <memory>
#include "Singleton.h"

namespace yy::core::rpc { class RpcServer; }
namespace yy::net { class EventLoop; }


namespace yy::app::center
{

class CenterServerManager final : public Singleton<CenterServerManager> {
    SINGLETON_NECESSITY(CenterServerManager)

public:
    void RunApp();

private:
    CenterServerManager();
    ~CenterServerManager() override;

    std::unique_ptr<core::rpc::RpcServer> m_rpcServer;
    std::unique_ptr<net::EventLoop> m_accpetorLoop;
};

}
