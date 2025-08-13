#include "CenterServerManager.h"

#include "AppXmlConfig.h"
#include "CenterServiceRpc_Impl.h"
#include "log.h"
#include "EventLoop.h"
#include "RpcServer.h"



using namespace std::chrono_literals;
using namespace yy::net;
using namespace yy::core;
using namespace yy::util;


namespace yy::app::center {

CenterServerManager::CenterServerManager():
    m_rpcServer{nullptr},
    m_accpetorLoop{nullptr}
{ }

CenterServerManager::~CenterServerManager() {
    m_rpcServer->Stop();
}

void CenterServerManager::RunApp()
{
    //! 读取配置文件
    config::ConfigManager::LoadXmlConfigs();
    //! 读取日志配置
    Ylog::LoggerManager::Instance().ReadConfigs();
    //! 初始化监听的端口和IP地址(IP地址未给出，则使用INADDR_ANY绑定所有IP地址)
    const IPAddressPtr rpcAddr = std::make_shared<IPv4Address>(config::g_app_config->GetValue().rpc_port());
    //! 初始化服务器对象
    m_accpetorLoop = std::make_unique<EventLoop>(500ms);
    m_rpcServer = std::make_unique<rpc::RpcServer>(m_accpetorLoop.get(), rpcAddr);
    m_rpcServer->RegisterService<CenterServiceRpc_Impl>(m_accpetorLoop.get());
    m_rpcServer->Start(2, 100ms);
    //! 启动监听线程(即主线程)并阻塞在此
    m_accpetorLoop->Loop();
}


}
