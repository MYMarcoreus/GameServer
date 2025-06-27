#include "EventLoop.h"
#include "log.h"
#include "LoginService.h"
#include "RpcServer.h"
#include "AppXmlConfig.h"


// using namespace yy::protocol::app;
// using namespace yy::app;
// using namespace yy::core;
// using namespace yy::net;

int main()
{
    yy::config::ConfigManager::AddFilePath("../config/configs_login.xml");
    yy::config::ConfigManager::AddFilePath("../../config/configs_login.xml");
    yy::config::ConfigManager::LoadXmlConfigs();
    yy::Ylog::LoggerManager::getInstance().ReadConfigs();

    yy::net::EventLoop loop{500ms};
    const yy::net::IPAddressPtr listenAddr = std::make_shared<yy::net::IPv4Address>(yy::config::g_app_config->GetValue().rpc_port());
    yy::core::RpcServer server(&loop, listenAddr);
    server.RegisterService(new yy::app::LoginService{});
    server.Start();
    loop.Loop();

    return 0;
}
