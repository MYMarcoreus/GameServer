#include "GateServerManager.h"
#include "log.h"
#include "EventLoop.h"
#include "GateServer.h"
#include "future"
#include "IPAddress.h"
#include "AccountRpcClient.h"
#include "RpcControllerImpl.h"
#include "GateRedisDAO.h"

#include <functional>

#include "ForwardManager.h"
#include "GateServiceRpc_Impl.h"
#include "RpcServer.h"

using namespace std::chrono_literals;
using namespace yy::net;
using namespace yy::core;
using namespace yy::util;
using yy::net::TcpConnectionPtr;



template<class T>
using Ptr = std::shared_ptr<T>;
using namespace yy::protocol::app;


namespace yy::app::gate {


GateServerManager::GateServerManager(): m_redisDAO(GateRedisDAO::Instance())
{
}

GateServerManager::~GateServerManager() {
    m_frontend->Stop();
    m_backend->Stop();
}


void GateServerManager::OnFrontend_Secutiry(const UserConnectionPtr& userconn) {
    userconn->SetState(UserConnection::E_UserBaseState::eSecure);
}

void GateServerManager::OnFrontend_Disconnect(const UserConnectionPtr& userconn) {
    YLOG_INFO("[GateServerManager::OnFrontend_Disconnect] 用户<{}>断开连接", userconn->GetUID())
    m_forwarder->OnFrontend_Disconnect(userconn);
}

void GateServerManager::OnFrontend_Message(const UserConnectionPtr& userconn, const MessagePtr& message, MessageNetType type)
{
    m_forwarder->ForwardToBackend(userconn, message, type);
}


void GateServerManager::RunApp()
{
    //! 读取配置文件
    config::ConfigManager::LoadXmlConfigs();

    //! 读取日志配置
    Ylog::LoggerManager::Instance().ReadConfigs();

    m_accpetorLoop = std::make_unique<EventLoop>(500ms);

    m_redisDAO.Start(m_accpetorLoop.get());

    /*********** 启动转发器 ***********/
    m_forwarder = std::make_unique<ForwardManager>(m_accpetorLoop.get());
    m_forwarder->Start();

    /*********** 初始化前端 ***********/
    //! 初始化监听的端口和IP地址
    IPAddressPtr frontend_tcp_addr = std::make_shared<IPv4Address>(
        "192.168.147.128", // 对外开放的地址
        config::g_app_config->GetValue().app_tcp_port()
    );
    IPAddressPtr frontend_udp_addr = std::make_shared<IPv4Address>(
        "192.168.147.128", // 对外开放的地址
        config::g_app_config->GetValue().app_udp_port()
    );
    //! 初始化服务器对象
    m_frontend = std::make_unique<GateServer>(m_accpetorLoop.get(), frontend_tcp_addr, frontend_udp_addr);
    m_frontend->SetNotifier_Security(
        [this](const UserConnectionPtr& userconn) {
            this->OnFrontend_Secutiry(userconn);
        });
    m_frontend->SetNotifier_DisConnect(
        [this](const UserConnectionPtr & userconn) {
            this->OnFrontend_Disconnect(userconn);
        });
    m_frontend->SetNotifier_Command(
        [this](const UserConnectionPtr & userconn, const MessagePtr & message, const MessageNetType type) {
            this->OnFrontend_Message(userconn, message, type);
        });

    /*********** 初始化后端 ***********/
    IPAddressPtr listenAddr_backend = std::make_shared<IPv4Address>(config::g_app_config->GetValue().rpc_port());
    m_backend = std::make_unique<rpc::RpcServer>(m_accpetorLoop.get(), listenAddr_backend);
    m_backend->RegisterService<GateServiceRpc_Impl>(m_accpetorLoop.get(), *m_forwarder);

    //! 启动服务器
    m_backend->Start(2, 500ms);
    m_frontend->Start();

    //! 启动监听线程(即主线程)的
    m_accpetorLoop->Loop();
}


}
