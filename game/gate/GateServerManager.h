#pragma once

#include "core_definations.h"
#include "CenterRpcClient.h"
#include "GameData.h"
#include "GateRedisDAO.h"
#include "ProtobufDispatcher.h"
#include "RpcStubConnectionPool.hpp"
#include "UserConnection.h"


namespace yy::app::gate
{
class ForwardManager;
}

namespace yy::core::rpc
{
class RpcServer;
}


namespace yy::app::gate
{
class GateRedisDAO;


class GateServerManager final : public Singleton<GateServerManager> {
    SINGLETON_NECESSITY(GateServerManager)
public:
    void RunApp();
    core::IServer& GetServer() const { return *m_frontend; }

private:
    GateServerManager();
    ~GateServerManager() override;

    void OnFrontend_Secutiry(const core::UserConnectionPtr& userconn) ;
    void OnFrontend_Disconnect(const core::UserConnectionPtr& userconn) ;
    void OnFrontend_Message(const core::UserConnectionPtr & userconn, const core::MessagePtr & message, core::MessageNetType type);

private:
    std::unique_ptr<net::EventLoop>         m_accpetorLoop;
    std::unique_ptr<core::IServer>          m_frontend;
    std::unique_ptr<core::rpc::RpcServer>   m_backend;
    std::unique_ptr<ForwardManager>         m_forwarder;
};


}
