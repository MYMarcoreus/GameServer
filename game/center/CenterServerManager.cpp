#include "CenterServerManager.h"
#include "log.h"
#include "EventLoop.h"
#include "RpcServer.h"
#include "AccountRpcServiceImpl.h"
#include "center.pb.h"

#include "CenterRpcServiceImpl.h"


using namespace std::chrono_literals;
using namespace yy::core;
using namespace yy::util;
using yy::net::TcpConnectionPtr;

using yy::protocol::app::SelectServerReq;
using yy::protocol::app::SelectServerRsp;


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
    //! ①、读取配置文件
    yy::config::ConfigManager::LoadXmlConfigs();
    //! ②、读取日志配置
    yy::Ylog::LoggerManager::Instance().ReadConfigs();


    //! ④、初始化监听的端口和IP地址(IP地址未给出，则使用INADDR_ANY绑定所有IP地址)
    const yy::net::IPAddressPtr rpcAddr = std::make_shared<net::IPv4Address>(config::g_app_config->GetValue().rpc_port());
    //! ⑤、初始化服务器对象（③和④）
    m_accpetorLoop = std::make_unique<net::EventLoop>(500ms);
    m_rpcServer = std::make_unique<core::rpc::RpcServer>(m_accpetorLoop.get(), rpcAddr);
    m_rpcServer->RegisterService<CenterRpcServiceImpl>(m_accpetorLoop.get());
    m_rpcServer->Start(2, 500ms);

    //! 启动监听线程(即主线程)并阻塞在此
    m_accpetorLoop->Loop();
}


}
