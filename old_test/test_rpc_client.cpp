#include "RpcStubConnectionPool.hpp"
#include "TcpConnection.h"
#include "EventLoop.h"
#include "log.h"
#include "rpc.pb.h"
#include "RemoteXmlConfig.h"
#include "account.pb.h"
#include "RpcControllerImpl.h"
#include "AccountRpcClient.h"

using yy::app::AccountRpcClient;

int main()
{
    yy::config::ConfigManager::AddFilePath("../config/configs_gate.xml");
    yy::config::ConfigManager::AddFilePath("../../config/configs_gate.xml");
    yy::config::ConfigManager::LoadXmlConfigs();

    START_YLOG_AFTER_CONFIG()

    yy::net::EventLoop loop{200ms};

    std::vector<std::unique_ptr<AccountRpcClient>> clients;
    for (int i = 0; i < 10; ++i) {
        auto client = std::make_unique<AccountRpcClient>();
        client->Start(10, [&client_raw = *client](const yy::net::TcpConnectionPtr & conn) {
            YLOG_INFO("连接至<{}:{}>，我方地址为<{}:{}>", conn->GetPeerAddr()->GetIPStr().c_str(), conn->GetPeerAddr()->GetPort()
                                                    , conn->GetLocalAddr()->GetIPStr().c_str(), conn->GetLocalAddr()->GetPort());
            const auto req = std::make_shared<yy::protocol::app::C2SLogin>();
            req->set_username("sadamofn");
            req->set_password("114514");
            req->set_session_id(101010);
            client_raw.CallRemoteAsync<yy::protocol::app::C2SLogin, yy::protocol::app::S2CLogin>(req,
                [](std::unique_ptr<yy::protocol::app::S2CLogin> && response, std::unique_ptr<yy::core::RpcControllerImpl> && controller) {
                    YLOG_INFO("回复：{}", response->token())
                });
        });
        clients.emplace_back(std::move(client));
    }

    loop.Loop();

    CLOSE_YLOG();
    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}