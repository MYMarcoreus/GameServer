#include "AccountServerManager.h"
#include "log.h"
#include "EventLoop.h"
#include "AccountServer.h"
#include "account.pb.h"
#include "RpcServer.h"
#include "AccountRpcServiceImpl.h"

#include <future>
#include <functional>


using namespace std::chrono_literals;
using namespace yy::core;
using namespace yy::util;
using yy::net::TcpConnectionPtr;

using yy::protocol::app::C2SLogin;
using yy::protocol::app::C2SRegister;


template<class T>
using Ptr = std::shared_ptr<T>;


namespace yy::app::account {


AccountServerManager::AccountServerManager():
      m_server{nullptr},
      m_dispatcher{[this](const core::UserConnectionPtr& userdata, const core::MessagePtr& message) { this->UnkonwnCommand(userdata, message); }},
      m_accpetorLoop{nullptr},
      m_wordThreads("Login Work Thread")
{
    // m_dispatcher.RegisterMessageCallback<XXXX>(
    //     [this](const UserConnectionPtr& user, const Ptr<XXXX>& msg) {
    //     });
}

AccountServerManager::~AccountServerManager() {
    m_server->Stop();
    m_rpcServer->Stop();
}


void AccountServerManager::AppNotifier_Secutiry(const core::UserConnectionPtr& userdata) {
    userdata->SetState(core::UserConnection::E_UserBaseState::eSecure);
}

void AccountServerManager::AppNotifier_Disconnect(const core::UserConnectionPtr& userdata) {
    YLOG_INFO("↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓ 用户<{}>断开连接", userdata->GetUID())
}

void AccountServerManager::AppNotifier_Command(const core::UserConnectionPtr & userdata, const core::MessagePtr & message, const MessageType type)
{
    m_wordThreads.PushTask([this, userdata, message](){
        m_dispatcher.OnProtobufMessage(userdata, message);
    });
}

void AccountServerManager::UnkonwnCommand(const core::UserConnectionPtr & userdata, const core::MessagePtr & message)
{
    YLOG_DEBUG("未知的消息类型：{}", message->GetDescriptor()->full_name())
    userdata->Shutdown();
}





void AccountServerManager::RunApp()
{
    //! 初始化服务器
    Init();

    //! 启动服务器的工作线程
    m_wordThreads.Start(m_accpetorLoop.get(), config::g_app_config->GetValue().work_thread_num());
    // this->m_wordThreads.RunTaskEvery( 8333us, [this](){this->m_server->Update();});

    //! 启动监听线程(即主线程)并阻塞在此
    m_accpetorLoop->Loop();
}


void AccountServerManager::Init()
{
    //! ①、读取配置文件
    yy::config::ConfigManager::LoadXmlConfigs();

    //! ②、读取日志配置
    yy::Ylog::LoggerManager::Instance().ReadConfigs();

    //! ③、初始化
    m_accpetorLoop = std::make_unique<net::EventLoop>(500ms);

    //! ④、初始化监听的端口和IP地址(IP地址未给出，则使用INADDR_ANY绑定所有IP地址)
    const yy::net::IPAddressPtr listenAddr = std::make_shared<net::IPv4Address>(
            config::g_app_config->GetValue().app_tcp_port());

    //! ⑤、初始化服务器对象（③和④）
    m_server = std::make_unique<AccountServer>(m_accpetorLoop.get(), listenAddr);
    m_server->SetNotifier_Security(
        [this](const core::UserConnectionPtr& userdata) {
            this->AppNotifier_Secutiry(userdata);
        });

    m_server->SetNotifier_DisConnect(
        [this](const core::UserConnectionPtr & userdata) {
            this->AppNotifier_Disconnect(userdata);
        });

    m_server->SetNotifier_Command(
        [this](const core::UserConnectionPtr & userdata, const core::MessagePtr & message, const MessageType type) {
            this->AppNotifier_Command(userdata, message, type);
        });

    //! 启动服务器的监听和IO线程
    m_server->Start();

    const yy::net::IPAddressPtr rpcAddr = std::make_shared<net::IPv4Address>(
            config::g_app_config->GetValue().rpc_port());
    m_rpcServer = std::make_unique<core::rpc::RpcServer>(m_accpetorLoop.get(), rpcAddr);
    m_rpcServer->RegisterService<AccountRpcServiceImpl>(m_accpetorLoop.get());
    m_rpcServer->Start(2, 500ms);
}


}
