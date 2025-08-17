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


// 业务层
namespace yy::app::logic {

class GameService;
class TestService;


class LogicServerManager final : public Singleton<LogicServerManager>
{
    SINGLETON_NECESSITY(LogicServerManager)
public:
    void RunApp();

    auto GetServer() const -> core::IServer& { return *m_frontend; }
    auto GetRpcServer() const -> core::rpc::RpcServer& { return *m_backend; }

private:
    LogicServerManager();
    ~LogicServerManager() override;

    std::unique_ptr<net::EventLoop>                  m_accpetorLoop{};
    std::unique_ptr<core::IServer>                   m_frontend{};
    std::unique_ptr<core::rpc::RpcServer>            m_backend{};
};


}

