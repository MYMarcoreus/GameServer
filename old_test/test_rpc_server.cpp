#include "EventLoop.h"
#include "log.h"
#include "AccountRpcServiceImpl.h"
#include "RpcServer.h"
#include "AppXmlConfig.h"
#include "ZkServiceManager.h"

#include "Endian.h"

int main()
{
    yy::config::ConfigManager::AddFilePath("../config/configs_account.xml");
    yy::config::ConfigManager::AddFilePath("../../config/configs_account.xml");
    yy::config::ConfigManager::LoadXmlConfigs();

    START_YLOG_AFTER_CONFIG()

    yy::net::EventLoop loop{500ms};
    const yy::net::IPAddressPtr listenAddr = std::make_shared<yy::net::IPv4Address>(yy::config::g_app_config->GetValue().rpc_port());
    yy::core::RpcServer server(&loop, listenAddr);
    server.RegisterService<yy::app::account::AccountRpcServiceImpl>(&loop);
    server.Start(3, 500ms);
    loop.Loop();

    CLOSE_YLOG();
    google::protobuf::ShutdownProtobufLibrary();

    std::cout << "---------main end---------" << std::endl;
    return 0;
}
