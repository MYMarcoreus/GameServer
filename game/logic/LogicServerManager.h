#pragma once

#include "CenterRpcClient.h"
#include "Singleton.h"
#include "core_definations.h"
#include "GameData.h"
#include "ProtobufDispatcher.h"
#include "RpcClient.hpp"


namespace yy::core::rpc
{
class RpcServer;
}

namespace yy::core::zk { class ZkServiceClient; }


// 业务层
namespace yy::app::logic {

class GameService;
class TestService;


class LogicServerManager final : public Singleton<LogicServerManager>
{
    SINGLETON_NECESSITY(LogicServerManager)
public:
    using F_CommandCallback = std::function<void(const UserConnectionPtr &, const MessagePtr &, core::MessageNetType)>;
    void RunApp();

    core::IServer& GetServer() const { return *m_frontend; }
    core::rpc::RpcServer&     GetRpcServer() const { return *m_backend; }

    void SetCommandCallback(F_CommandCallback cb) { m_cmdCallback = std::move(cb); }

private:
    LogicServerManager();
    ~LogicServerManager() override;

    void Init();

    std::unique_ptr<net::EventLoop>                  m_accpetorLoop{};
    std::unique_ptr<core::IServer>                   m_frontend{};
    std::unique_ptr<core::rpc::RpcServer>            m_backend{};
    F_CommandCallback                                m_cmdCallback{};

    std::unique_ptr<core::zk::ZkServiceClient>       m_zk{};
    std::unique_ptr<rpc_client::CenterRpcClient>     m_centerRpcClient{};
};


}

