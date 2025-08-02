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

using namespace yy::net;
using namespace yy::core;

// 业务层
namespace yy::app::logic {

class GameService;
class TestService;


class LogicServerManager final : public Singleton<LogicServerManager>
{
    SINGLETON_NECESSITY(LogicServerManager)
public:
    using F_CommandCallback = std::function<void(const UserConnectionPtr &, const MessagePtr &, MessageNetType)>;
    void RunApp();

    IServer& GetServer() const { return *m_server; }
    rpc::RpcServer&     GetRpcServer() const { return *m_rpcServer; }

    void SetCommandCallback(F_CommandCallback cb) { m_cmdCallback = std::move(cb); }

private:
    LogicServerManager();
    ~LogicServerManager() override;

    void Init();

    void AppNotifier_Secutiry(const UserConnectionPtr& userdata) ;
    void AppNotifier_Disconnect(const UserConnectionPtr& userdata) ;

    unique_ptr<EventLoop>                   m_accpetorLoop{};
    unique_ptr<IServer>                     m_server{};
    unique_ptr<rpc::RpcServer>              m_rpcServer{};
    F_CommandCallback                       m_cmdCallback{};

    unique_ptr<zk::ZkServiceClient>         m_zk{};
    unique_ptr<rpc_client::CenterRpcClient> m_centerRpcClient{};
};


}

