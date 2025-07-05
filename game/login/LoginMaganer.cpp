#include "LoginMaganer.h"
#include "log.h"
#include "EventLoop.h"
#include "LoginServer.h"
#include "login.pb.h"
#include <future>
#include <functional>

using namespace std::chrono_literals;
using namespace yy::core;
using namespace yy::util;
using yy::net::TcpConnectionPtr;

using yy::protocol::app::C2SLogin;


template<class T>
using Ptr = std::shared_ptr<T>;


namespace yy::app::login {


LoginManager::LoginManager():
      m_server{},
      m_dispatcher{[this](const core::UserConnectionPtr& userdata, const core::MessagePtr& message) { this->UnkonwnCommand(userdata, message); }},
      m_accpetorLoop{},
      m_wordThreads("Login Work Thread")
{
    m_dispatcher.RegisterMessageCallback<C2SLogin>(
        [this](const UserConnectionPtr& user, const Ptr<C2SLogin>& msg) {
            // this->OnLogin(user, msg);
        });
}

LoginManager::~LoginManager() {
    m_server->Stop();

    delete m_server;
    delete m_accpetorLoop;
}


void LoginManager::AppNotifier_Secutiry(const core::UserConnectionPtr& userdata) {
    userdata->SetState(core::UserConnection::E_UserBaseState::eSecure);
}

void LoginManager::AppNotifier_Disconnect(const core::UserConnectionPtr& userdata) {
    YLOG_INFO("↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓ 用户<{}>断开连接", userdata->GetUID())
}

void LoginManager::AppNotifier_Command(const core::UserConnectionPtr & userdata, const core::MessagePtr & message)
{
    m_wordThreads.PushTask([this, userdata, message](){
        m_dispatcher.OnProtobufMessage(userdata, message);
    });
}

void LoginManager::UnkonwnCommand(const core::UserConnectionPtr & userdata, const core::MessagePtr & message)
{
    YLOG_DEBUG("未知的消息类型：{}", message->GetDescriptor()->full_name())
    userdata->Shutdown();
}





void LoginManager::RunApp()
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


void LoginManager::Init()
{
    //! ①、读取配置文件
    yy::config::ConfigManager::LoadXmlConfigs();

    //! ②、读取日志配置
    yy::Ylog::LoggerManager::Instance().ReadConfigs();

    //! ③、初始化
    m_accpetorLoop = new net::EventLoop(500ms);

    //! ④、初始化监听的端口和IP地址(IP地址未给出，则使用INADDR_ANY绑定所有IP地址)
    yy::net::IPAddressPtr listenAddr = std::make_shared<net::IPv4Address>(
            config::g_app_config->GetValue().app_tcp_port());

    //! ⑤、初始化服务器对象（③和④）
    m_server = new LoginServer(m_accpetorLoop, listenAddr);
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

void LoginManager::StartListenAndIOLoop()
{
    m_server->Start();
}


}
