#include "LogicServerManager.h"
#include "GameService.h"
#include "log.h"
#include "EventLoop.h"
#include "LogicServer.h"
#include "RpcServer.h"
#include "UserConnection.h"

using namespace std::chrono_literals;
using namespace yy::net;
using namespace yy::core;

namespace yy::app::logic {


LogicServerManager::LogicServerManager()
{
}

LogicServerManager::~LogicServerManager() {
    m_frontend->Stop();
}

void LogicServerManager::RunApp()
{
    //! ①、读取配置文件
    config::ConfigManager::LoadXmlConfigs();

    //! ②、读取日志配置
    Ylog::LoggerManager::Instance().ReadConfigs();

    //! ③、初始化
    m_accpetorLoop = make_unique<EventLoop>(500ms);

    //! ④、初始化前端监听的IP地址（使用配置端口）
    const IPAddressPtr frontend_tcp_addr = std::make_shared<IPv4Address>(config::g_app_config->GetValue().app_tcp_port());
    const IPAddressPtr frontend_udp_addr = std::make_shared<IPv4Address>(config::g_app_config->GetValue().app_udp_port());
    //! ⑤、初始化前端服务器
    m_frontend = make_unique<LogicServer>(m_accpetorLoop.get(), frontend_tcp_addr, frontend_udp_addr);
    m_frontend->SetNotifier_Security(
        [this](const UserConnectionPtr& userdata) {
            YLOG_INFO("连接安全验证通过<{}>", userdata->GetConnID())
        });

    //! ⑥、初始化后端监听的IP地址（使用配置rpc端口）
    const IPAddressPtr backend_addr = std::make_shared<IPv4Address>(config::g_app_config->GetValue().rpc_port());
    //! ⑤、初始化后端服务器，注册RPC地址到zookeeper中
    m_backend = std::make_unique<rpc::RpcServer>(m_accpetorLoop.get(), backend_addr);
    m_backend->RegisterService<GameService>(m_accpetorLoop.get(), *m_frontend);

    //! 启动服务器的监听和IO线程
    m_backend->Start(2, 500ms);
    m_frontend->Start();

    //! 启动监听线程(即主线程)
    m_accpetorLoop->Loop();

    //! 事件循环退出（收到 SIGINT/SIGTERM 等信号），显式停止后端并注销 ZooKeeper 服务节点
    if (m_backend) {
        m_backend->Stop();
    }
    if (m_frontend) {
        m_frontend->Stop();
    }
}


}
