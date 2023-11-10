/*
#include "GameManager.h"
#include "log.h"
#include "LogXmlConfig.h"
#include "ConfigManager.h"
#include <sstream>

int main()
{
    setbuf(stdout, nullptr);
    yy::app::GameManager::StartApp();
}
*/

#include "GameServer.h"
#include "EventLoop.h"
#include "ConfigManager.h"

using namespace yy::core;
using namespace yy::net;
using namespace yy::config;

int main()
{
    ConfigManager::LoadConfigs();
    EventLoop loop{true};
    IPAddressPtr listenAddr = std::make_shared<IPv4Address>(g_app_config->GetValue().app_port());
    GameServer server(&loop, listenAddr);
    server.Start();
    loop.Loop();



    return 0;
}