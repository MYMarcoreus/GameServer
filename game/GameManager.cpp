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




void GameManager::AppNotifier_Secutiry(core::UserConnectionPtr userdata) {
    userdata->SetState(core::UserConnection::E_UserBaseState::eSecure);
}

void GameManager::AppNotifier_Disconnect(core::UserConnectionPtr userdata) {
    YLOG_INFO("↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓ 用户<{}>断开连接", userdata->GetUID())

    // 已登陆，保存数据
    if(userdata->IsLoggedIn())
    {
        //! 被动离开时执行
        YLOG_INFO("<{}> Saving Data Now!", userdata->GetSocketFD())
        m_player->LeaveAndSave(userdata);
        YLOG_INFO("<{}> User Data Saved!", userdata->GetSocketFD())
    }
    else // 未登录，重置数据
    {
        YLOG_INFO("<{}> DataReset", userdata->GetSocketFD())
        userdata->Shutdown();
    }
}

void GameManager::AppNotifier_Command(const core::UserConnectionPtr & userdata, const core::MessagePtr & message)
{
    //! 对于游戏游戏，并不在IO线程处理，而是在专门处理游戏数据的工作线程中处理（让Game层的分发器找到该游戏消息所注册的对应的处理函数。）
    m_wordThreads.PushTask([this, userdata, message](){
        m_dispatcher.OnProtobufMessage(userdata, message);
    });
}

void GameManager::UnkonwnCommand(const core::UserConnectionPtr & userdata, const core::MessagePtr & message)
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
    m_wordThreads.Start(m_accpetorLoop, config::g_app_config->GetValue().work_thread_num());
    // this->m_wordThreads.RunTaskEvery( 8333us, [this](){this->m_server->Update();});

    //! 启动监听线程(即主线程)的
    m_accpetorLoop->Loop();
}


void GameManager::Init()
{
    //! ①、读取配置文件
    yy::config::ConfigManager::LoadXmlConfigs();

    //! ②、读取日志配置
    yy::Ylog::LoggerManager::getInstance().ReadConfigs();

    //! ③、初始化
    m_accpetorLoop = new net::EventLoop(500ms);

    //! ④、初始化监听的端口和IP地址(IP地址未给出，则使用INADDR_ANY绑定所有IP地址)
    yy::net::IPAddressPtr listenAddr = std::make_shared<net::IPv4Address>(
            config::g_app_config->GetValue().app_tcp_port());

    //! ⑤、初始化服务器对象（③和④）
    m_server = new core::GameServer(m_accpetorLoop, listenAddr);
    m_server->SetNotifier_Security(std::bind(&GameManager::AppNotifier_Secutiry, this, _1));
    m_server->SetNotifier_DisConnect(std::bind(&GameManager::AppNotifier_Disconnect, this, _1));
    m_server->SetNotifier_Command(std::bind(&GameManager::AppNotifier_Command, this, _1, _2));

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
    : m_wordThreads("Game Work Thread"),
      m_server{},
      m_player{},
      m_test{},
      m_dispatcher{std::bind(&GameManager::UnkonwnCommand, this, _1, _2)},
      m_accpetorLoop{}
{

}

GameManager::~GameManager() {
    m_server->Stop();

    delete m_server;
    delete m_accpetorLoop;
}

}
