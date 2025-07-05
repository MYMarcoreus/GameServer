#include "EventLoop.h"
#include "log.h"
#include "LoginRpcService.h"
#include "RpcServer.h"
#include "AppXmlConfig.h"
#include "ZkServiceManager.h"


int main()
{
    yy::config::ConfigManager::AddFilePath("../config/configs_login.xml");
    yy::config::ConfigManager::AddFilePath("../../config/configs_login.xml");
    yy::config::ConfigManager::LoadXmlConfigs();

    START_YLOG_AFTER_CONFIG()

    yy::net::EventLoop loop{500ms};
    const yy::net::IPAddressPtr listenAddr = std::make_shared<yy::net::IPv4Address>(yy::config::g_app_config->GetValue().rpc_port());
    yy::core::RpcServer server(&loop, listenAddr);
    server.RegisterService<yy::app::login::LoginRpcService>();
    server.Start();
    loop.Loop();

    CLOSE_YLOG();
    google::protobuf::ShutdownProtobufLibrary();

    std::cout << "---------main end---------" << std::endl;
    return 0;
}
