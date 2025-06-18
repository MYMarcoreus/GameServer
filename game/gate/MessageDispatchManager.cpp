#include "MessageDispatchManager.h"
#include "log.h"
#include "EventLoop.h"
#include "GateServer.h"
#include "future"
#include <functional>

using namespace std::chrono_literals;
using namespace yy::core;
using namespace yy::util;
using yy::net::TcpConnectionPtr;

using yy::protocol::app::C2SEnterScene;
using yy::protocol::app::C2SOtherPlayerData;
using yy::protocol::app::C2SMove;
using yy::protocol::app::C2SJumpAndGravity;
using yy::protocol::app::C2SPlayerLeave;

using yy::protocol::app::S2CMove;
using yy::protocol::app::S2CJumpAndGravity;

using yy::protocol::app::S2COtherPlayerData;
using yy::protocol::app::PlayerBaseData;
using yy::protocol::app::PlayerMove;



namespace yy::app {


MessageDispatchManager::MessageDispatchManager():
      m_server{},
      m_dispatcher{[this](const core::UserConnectionPtr& userdata, const core::MessagePtr& message) { this->UnkonwnCommand(userdata, message); }},
      m_accpetorLoop{},
      m_wordThreads("Gate Work Thread")
{
    m_dispatcher.RegisterMessageCallback<C2SEnterScene>(
        [this](const UserConnectionPtr& user, const Ptr<C2SEnterScene>& msg) {
            // this->OnLogin(user, msg);
        });
    m_dispatcher.RegisterMessageCallback<C2SOtherPlayerData>(
        [this](const UserConnectionPtr& user, const Ptr<C2SOtherPlayerData>& msg) {
            // this->OnC2SOtherPlayerData(user, msg);
        });
    m_dispatcher.RegisterMessageCallback<C2SMove>(
        [this](const UserConnectionPtr& user, const Ptr<C2SMove>& msg) {
            // this->OnC2SMove(user, msg);
        });
    m_dispatcher.RegisterMessageCallback<C2SJumpAndGravity>(
        [this](const UserConnectionPtr& user, const Ptr<C2SJumpAndGravity>& msg) {
            // this->OnC2SJumpAndGravity(user, msg);
        });
    m_dispatcher.RegisterMessageCallback<C2SPlayerLeave>(
        [this](const UserConnectionPtr& user, const Ptr<C2SPlayerLeave>& msg) {
            // this->OnLeave(user, msg);
        });
}

MessageDispatchManager::~MessageDispatchManager() {
    m_server->Stop();

    delete m_server;
    delete m_accpetorLoop;
}


void MessageDispatchManager::AppNotifier_Secutiry(const core::UserConnectionPtr& userdata) {
    userdata->SetState(core::UserConnection::E_UserBaseState::eSecure);
}

void MessageDispatchManager::AppNotifier_Disconnect(const core::UserConnectionPtr& userdata) {
    YLOG_INFO("↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓ 用户<{}>断开连接", userdata->GetUID())

    // // 已登陆，保存数据
    // if(userdata->IsLoggedIn())
    // {
    //     //! 被动离开时执行
    //     YLOG_INFO("<{}> Saving Data Now!", userdata->GetSocketFD())
    //     m_player->LeaveAndSave(userdata);
    //     YLOG_INFO("<{}> User Data Saved!", userdata->GetSocketFD())
    // }
    // else // 未登录，重置数据
    // {
    //     YLOG_INFO("<{}> DataReset", userdata->GetSocketFD())
    //     userdata->Shutdown();
    // }
}

void MessageDispatchManager::AppNotifier_Command(const core::UserConnectionPtr & userdata, const core::MessagePtr & message)
{
    //! 对于游戏游戏，并不在IO线程处理，而是在专门处理游戏数据的工作线程中处理（让Game层的分发器找到该游戏消息所注册的对应的处理函数。）
    m_wordThreads.PushTask([this, userdata, message](){
        m_dispatcher.OnProtobufMessage(userdata, message);
    });
}

void MessageDispatchManager::UnkonwnCommand(const core::UserConnectionPtr & userdata, const core::MessagePtr & message)
{
    YLOG_DEBUG("未知的消息类型：{}", message->GetDescriptor()->full_name())
    userdata->Shutdown();
}





void MessageDispatchManager::RunApp()
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


void MessageDispatchManager::Init()
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
    m_server = new GateServer(m_accpetorLoop, listenAddr);
    m_server->SetNotifier_Security(
        [this](const core::UserConnectionPtr& userdata) {
            this->AppNotifier_Secutiry(userdata);
        });

    m_server->SetNotifier_DisConnect(
        [this](const core::UserConnectionPtr & userdata) {
            this->AppNotifier_Disconnect(userdata);
        });

    m_server->SetNotifier_Command(
        [this](const core::UserConnectionPtr & userdata, const core::MessagePtr & message) {
            this->AppNotifier_Command(userdata, message);
        });
}

void MessageDispatchManager::StartListenAndIOLoop()
{
    m_server->Start();
}


}
