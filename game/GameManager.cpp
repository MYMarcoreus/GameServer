#include "GameManager.h"
#include "GamePlayerManager.h"
#include "GameTestManager.h"
#include "log.h"
#include "EventLoop.h"
#include "GameServer.h"
#include "future"
#include <functional>

using namespace std::chrono_literals;

namespace yy::app {




void GameManager::AppNotifier_Secutiry(const yy::net::TcpConnectionPtr& conn) {
    // YLOG_TRACE("in AppNotifier_Secutiry")
    // YLOG_INFO("用户<{}>安全验证通过：{}", userdata->sock.get_fd(), result_code)
}

void GameManager::AppNotifier_Disconnect(const yy::net::TcpConnectionPtr& conn) {
    YLOG_INFO("用户<{},{}>断开连接", conn->GetSocketFD(), conn->GetName())

    auto userdata = m_server->FindUser(conn->GetName());

    // 已登陆，保存数据
    if(userdata->isLoggedIn())
    {
        //! 被动离开时执行
        YLOG_INFO("<{}> Saving Data Now!", conn->GetSocketFD())
        m_player->LeaveAndSave(userdata);
        YLOG_INFO("<{}> USer Data Saved!", conn->GetSocketFD())
    }
    else // 未登录，重置数据
    {
        YLOG_INFO("<{}> DataReset", conn->GetSocketFD())
        userdata->Shutdown();
    }
}

void GameManager::AppNotifier_Command(const core::UserBaseDataPtr & userdata, const core::MessagePtr & message)
{
    m_dispatcher.OnProtobufMessage(userdata, message);
}

void GameManager::UnkonwnCommand(const core::UserBaseDataPtr & userdata, const core::MessagePtr & message)
{
    YLOG_DEBUG("未知的消息类型：{}", message->GetDescriptor()->full_name())
    userdata->Shutdown();
}





void GameManager::RunApp()
{
    //! 初始化服务器
    Init();

    //! 启动服务器的监听和IO线程
    StartListenAndIOLoop();

    //! 启动服务器的工作线程
    m_wordThreads.Start(m_loop, 2);
    this->m_wordThreads.RunTaskEvery( 8333us, [this](){this->m_server->Update();});

    m_loop->Loop();
}


void GameManager::Init()
{
    yy::config::ConfigManager::LoadConfigs();
    m_loop = new net::EventLoop();
    yy::net::IPAddressPtr listenAddr = std::make_shared<net::IPv4Address>(config::g_app_config->GetValue().app_port());
    m_server = new core::GameServer(m_loop, listenAddr);
    m_server->setNotifier_Security  (std::bind(&GameManager::AppNotifier_Secutiry, this, _1) );
    m_server->setNotifier_DisConnect(std::bind(&GameManager::AppNotifier_Disconnect, this, _1));
    m_server->setNotifier_Command   (std::bind(&GameManager::AppNotifier_Command, this, _1, _2));



    m_player = &GamePlayerManager::getInstance();
    m_player->Init();

    m_test = &GameTestManager::getInstance();
    m_test->Init();
}

void GameManager::StartListenAndIOLoop()
{
    m_server->Start();
}

GameManager::GameManager()
    : m_server{},
    m_player{},
    m_test{},
    m_dispatcher{std::bind(&GameManager::UnkonwnCommand, this, _1, _2)},
    m_loop{}
{

}

GameManager::~GameManager() {
    m_server->Stop();

    delete m_server;
    delete m_loop;
}

}