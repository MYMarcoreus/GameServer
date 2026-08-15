#include "AccountServerManager.h"
#include "log.h"
#include "EventLoop.h"
#include "AccountServer.h"
#include "RpcServer.h"
#include "AccountServiceRpc_Impl.h"
#include "ThreadPool.h"

using namespace std::chrono_literals;
using namespace yy::net;
using namespace yy::core;
using namespace yy::util;

template<class T>
using Ptr = std::shared_ptr<T>;


namespace yy::app::account {


AccountServerManager::AccountServerManager():
      m_accpetorLoop{nullptr}
{ }

AccountServerManager::~AccountServerManager() {
    m_rpcServer->Stop();
}

void AccountServerManager::RunApp()
{
    //! ①、读取配置文件
    config::ConfigManager::LoadXmlConfigs();

    //! ②、读取日志配置
    Ylog::LoggerManager::Instance().ReadConfigs();

    //! ③、初始化
    m_accpetorLoop = std::make_unique<EventLoop>(500ms);

    //! ④、初始化监听的端口和IP地址(IP地址未给出，则使用INADDR_ANY绑定所有IP地址)
    const IPAddressPtr rpcAddr = std::make_shared<IPv4Address>(config::g_app_config->GetValue().rpc_port());

    //! ⑤、初始化服务器对象（③和④）
    m_rpcServer = std::make_unique<core::rpc::RpcServer>(m_accpetorLoop.get(), rpcAddr);
    m_rpcServer->RegisterService<AccountServiceRpc_Impl>(m_accpetorLoop.get());
    m_rpcServer->Start(2, 500ms);

    //! 启动监听线程(即主线程)并阻塞在此
    m_accpetorLoop->Loop();
}


}
