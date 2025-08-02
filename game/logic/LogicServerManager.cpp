#include "LogicServerManager.h"
#include "GameService.h"
#include "TestService.h"
#include "log.h"
#include "EventLoop.h"
#include "LogicServer.h"
#include "ZkServiceClient.h"
#include "RpcServer.h"
#include <future>
#include <functional>

using namespace std::chrono_literals;

namespace yy::app::logic {


LogicServerManager::LogicServerManager() :
    m_zk(std::make_unique<zk::ZkServiceClient>())
{
}

LogicServerManager::~LogicServerManager() {
    m_server->Stop();
}

void LogicServerManager::AppNotifier_Secutiry(const UserConnectionPtr& userdata) {
    userdata->SetState(UserConnection::E_UserBaseState::eSecure);
}

void LogicServerManager::AppNotifier_Disconnect(const UserConnectionPtr& userdata) {
    YLOG_INFO("↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓ 用户<{}>断开连接", userdata->GetUID())

    // 已登陆，保存数据
    if(userdata->IsLoggedIn())
    {
        //! 被动离开时执行
        YLOG_INFO("<{}> No Data Saved Now!", userdata->GetSocketFD())
    }
    else // 未登录，重置数据
    {
        YLOG_INFO("<{}> DataReset", userdata->GetSocketFD())
        userdata->Shutdown();
    }
}

void LogicServerManager::RunApp()
{
    //! 初始化服务器
    Init();

    //! 启动监听线程(即主线程)的
    m_accpetorLoop->Loop();
}


void LogicServerManager::Init()
{
    //! ①、读取配置文件
    config::ConfigManager::LoadXmlConfigs();

    //! ②、读取日志配置
    Ylog::LoggerManager::Instance().ReadConfigs();

    //! ③、初始化
    m_accpetorLoop = make_unique<EventLoop>(500ms);

    //! ④、初始化监听的端口和IP地址(IP地址未给出，则使用INADDR_ANY绑定所有IP地址)
    const IPAddressPtr listenAddr = std::make_shared<IPv4Address>(
            "192.168.147.128",
            config::g_app_config->GetValue().app_tcp_port()
        );

    //! ⑤、初始化服务器对象（③和④）
    m_server = make_unique<LogicServer>(m_accpetorLoop.get(), listenAddr);
    m_server->SetNotifier_Security(
        [this](const UserConnectionPtr& userdata) {
            this->AppNotifier_Secutiry(userdata);
        });
    m_server->SetNotifier_DisConnect(
        [this](const UserConnectionPtr & userdata) {
            this->AppNotifier_Disconnect(userdata);
        });

    m_zk->Start("/services");
    m_zk->Register("GameService", listenAddr->GetIPStr(), listenAddr->GetPortStr());

    //! 启动服务器的监听和IO线程
    m_server->Start();

    const IPAddressPtr rpcAddr = std::make_shared<IPv4Address>(config::g_app_config->GetValue().rpc_port());
    m_rpcServer = std::make_unique<rpc::RpcServer>(m_accpetorLoop.get(), rpcAddr);
    m_rpcServer->RegisterService<GameService>(m_accpetorLoop.get());
    m_rpcServer->Start(2, 500ms);
}


}
