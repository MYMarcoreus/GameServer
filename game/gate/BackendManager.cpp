#include "BackendManager.h"

namespace yy::app::gate
{

thread_local decltype(BackendManager::t_backend) BackendManager::t_backend;


BackendSession::BackendSession(net::EventLoop* loop, const net::IPAddressPtr& serverAddr):
    loop_{loop},
    client_(loop,
            yy::config::g_remote_config->GetValue().sendBytesOne,
            yy::config::g_remote_config->GetValue().sendBytesMax,
            yy::config::g_remote_config->GetValue().recvBytesOne,
            yy::config::g_remote_config->GetValue().recvBytesMax,
            yy::config::g_remote_config->GetValue().appXorCode,
            serverAddr),
    codec_([this](const net::TcpConnectionPtr& conn, const core::MessagePtr & buf) {
        //
    })
{
    client_.SetMessageCallback(
        [this](const net::TcpConnectionPtr& conn, net::NetBuffer & buf) {
            codec_.OnTcpData(conn, buf);
        });

    client_.SetConnectionEstablishedCallback(
        [this](const net::TcpConnectionPtr& conn) {
            //
        });

    client_.SetCanAutoRetry(true);
}

void BackendSession::Connect(const net::IPAddressPtr& server_addr)
{
    client_.Connect(server_addr);
}

void BackendSession::OnBackendResponse(const net::TcpConnectionPtr& conn, const core::MessagePtr& buf)
{

}

BackendManager::BackendManager():
    m_accountRpcClient{}
{
}

void BackendManager::Init(net::EventLoop * loop)
{
    //! 服务发现：读取其他服务器的地址
    m_zk.Start(kServiceRoot);
    m_routeTable = m_zk.FetchAllRemote();

    // 与其它服务器建立连接
    for (auto & [service_name, service_addrs]: m_routeTable) {
        for (const auto & service_addr: service_addrs) {
            YLOG_INFO("[GateServer Service Discover] {} in {}:{}", service_name, service_addr->GetIPStr(), service_addr->GetPort());
            const auto backend_session = std::make_unique<BackendSession>(loop, service_addr);
            backend_session->Connect();
            // m_backends[service_name].emplace_back(std::move(backend_session));
        }
        m_zk.Watch(service_name, [](const std::string & service_path, std::vector<yy::net::IPAddressPtr> && node) {
            //监听服务地址，改变时
        });
    }
}

void BackendManager::OnFrontendMessage(const core::UserConnectionPtr& userconn, const core::MessagePtr& message, const core::MessageType type)
{
    // 转发前端消息到后端
}
}
