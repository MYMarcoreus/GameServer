#include "GateServerManager.h"
#include "log.h"
#include "EventLoop.h"
#include "GateServer.h"
#include "player.pb.h"
#include "future"
#include "IPAddress.h"
#include "AccountRpcClient.h"

#include <functional>

using namespace std::chrono_literals;
using namespace yy::core;
using namespace yy::util;
using yy::net::TcpConnectionPtr;

using yy::protocol::app::C2SLogin;
using yy::protocol::app::C2SRegister;
using yy::protocol::app::S2CLogin;
using yy::protocol::app::S2CRegister;


template<class T>
using Ptr = std::shared_ptr<T>;


namespace yy::app::gate {


GateServerManager::GateServerManager():
    m_accpetorLoop{std::make_unique<yy::net::EventLoop>(500ms)},
    m_dispatcher{[this](const core::UserConnectionPtr& userconn, const core::MessagePtr& message) { this->UnkonwnCommand(userconn, message); }},
    m_workThreads("Gate Work Thread")
{
    RegisterRpcForward<C2SLogin, S2CLogin>();
    RegisterRpcForward<C2SRegister, S2CRegister>();
}

GateServerManager::~GateServerManager() {
    m_frontend->Stop();
}


void GateServerManager::OnFrontend_Secutiry(const core::UserConnectionPtr& userconn) {
    userconn->SetState(core::UserConnection::E_UserBaseState::eSecure);
}

void GateServerManager::OnFrontend_Disconnect(const core::UserConnectionPtr& userconn) {
    YLOG_INFO("↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓ 用户<{}>断开连接", userconn->GetUID())
}

void GateServerManager::OnFrontend_Message(const core::UserConnectionPtr & userconn, const core::MessagePtr & message, const core::MessageType type)
{
    m_workThreads.PushTask([this, userconn, message](){
        m_dispatcher.OnProtobufMessage(userconn, message);
    });
}

void GateServerManager::UnkonwnCommand(const core::UserConnectionPtr & userconn, const core::MessagePtr & message)
{
    YLOG_DEBUG("未知的消息类型：{}", message->GetDescriptor()->full_name())
    userconn->Shutdown();
}





void GateServerManager::RunApp()
{
    //! 初始化服务器
    Init();

    //! 启动服务器的工作线程
    m_workThreads.Start(m_accpetorLoop.get(), config::g_app_config->GetValue().work_thread_num());

    //! 启动监听线程(即主线程)的
    m_accpetorLoop->Loop();
}

void GateServerManager::TestRpcConnectionEstablished(const yy::net::TcpConnectionPtr& conn)
{
    YLOG_INFO("连接至<{}:{}>，我方地址为<{}:{}>", conn->GetPeerAddr()->GetIPStr().c_str(), conn->GetPeerAddr()->GetPort()
                                            , conn->GetLocalAddr()->GetIPStr().c_str(), conn->GetLocalAddr()->GetPort());

    auto req = std::make_shared<yy::protocol::app::C2SLogin>();
    req->set_username("sadamofn");
    req->set_password("114514");
    req->set_session_id(101010);
    m_accountRpcClient->CallRemoteAsync<yy::protocol::app::C2SLogin, yy::protocol::app::S2CLogin>(req,
        [](std::unique_ptr<yy::protocol::app::S2CLogin> && response, std::unique_ptr<yy::core::RpcControllerImpl> && controller) {
            YLOG_INFO("回复：{}", response->token())
        });
}


void GateServerManager::Init()
{
    //! 读取配置文件
    yy::config::ConfigManager::LoadXmlConfigs();

    //! 读取日志配置
    yy::Ylog::LoggerManager::Instance().ReadConfigs();

    m_accountRpcClient = std::make_unique<AccountRpcClient>();
    m_accountRpcClient->Start(10,
        [this](const net::TcpConnectionPtr & conn) {
            this->TestRpcConnectionEstablished(conn);
        }
    );

    /*********** 启动前端 ***********/
    //! 初始化监听的端口和IP地址(IP地址未给出，则使用INADDR_ANY绑定所有IP地址)
    yy::net::IPAddressPtr listenAddr = std::make_shared<net::IPv4Address>(config::g_app_config->GetValue().app_tcp_port());

    //! 初始化服务器对象
    m_frontend = std::make_unique<GateServer>(m_accpetorLoop.get(), listenAddr);
    m_frontend->SetNotifier_Security(
        [this](const core::UserConnectionPtr& userconn) {
            this->OnFrontend_Secutiry(userconn);
        });
    m_frontend->SetNotifier_DisConnect(
        [this](const core::UserConnectionPtr & userconn) {
            this->OnFrontend_Disconnect(userconn);
        });
    m_frontend->SetNotifier_Command(
        [this](const core::UserConnectionPtr & userconn, const core::MessagePtr & message, const core::MessageType type) {
            this->OnFrontend_Message(userconn, message, type);
        });

    //! 启动服务器的监听和IO线程
    m_frontend->Start();
}



}
