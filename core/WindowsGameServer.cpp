#include "WindowsGameServer.h"
#include "TcpConnection.h"
#include "log.h"
#include "connection.pb.h"

#include <google/protobuf/message.h>

using namespace yy::net;
using namespace yy::config;

namespace yy::core {


WindowsGameServer::WindowsGameServer(EventLoop *loop, IPAddressPtr listenAddr)
        : m_loop{loop},
          m_server(loop, listenAddr, true),
          m_dispatcher( std::bind(&WindowsGameServer::OnUnknownMessage, this, _1, _2) ),
          m_codec(std::bind(&ProtobufDispatcher::OnProtobufMessage, &m_dispatcher, _1, _2)),
          m_app_configvar(g_app_config)
{
    // m_dispatcher.RegisterMessageCallback<Query>(std::bind(&QueryServer::OnQuery, this, _1, _2));
    m_server.SetMessageCallback( std::bind(&ProtobufCodec::OnMessage, &m_codec, _1, _2));
    m_server.SetConnectionEstablishedCallback( std::bind(&WindowsGameServer::OnConnectionEstablished, this, _1));
    m_server.SetConnectionShutdownCallback([this](const TcpConnectionPtr & conn) { this->AddShutdownConnection(conn); });
    m_server.SetCloseSocketsCallback([this]() { this->CloseShutdownConnections(); });
}

void WindowsGameServer::OnUnknownMessage(TcpConnectionPtr conn, const MessagePtr &message) {
    YLOG_INFO("未知的消息类型：{}", message->GetDescriptor()->full_name());
}




void WindowsGameServer::OnConnectionEstablished(TcpConnectionPtr conn) {
    YLOG_INFO("███████████████████连接成功<{}:{}, {}>！",
              conn->GetPeerAddr()->GetIPStr().c_str(), conn->GetPeerAddr()->GetPort(), conn->GetSocketFD());

    SendXorCode(conn);
}

void WindowsGameServer::SendXorCode(const TcpConnectionPtr &conn) {
    // 发送随机生成的异或码给用户，之后的通信都用该异或码进行加密
    auto gen_val = MessageHeader::GenerateXorCode();
    yy::core::protocol::XorBody xorBody;
    xorBody.set_xor_code(gen_val ^ m_app_configvar->GetValue().app_xor_code()); //! 记得与初始异或码异或

    conn->SetXorCode(gen_val);
    conn->Send(xorBody);
    YLOG_TRACE("Thread_Accepter: 封包异或码<%d,%d>给用户<%d>", gen_val,xorBody.xor_code(), conn->GetSocketFD())
}


void WindowsGameServer::AddShutdownConnection(const TcpConnectionPtr & conn) {
    {
        std::lock_guard lg{m_ShutdownConnectionsMutex};
        m_ShutdownConnections.push_back(conn);
    }
    YLOG_TRACE("In TcpServer::AddShutdownConnection<%d>", conn->GetSocketFD());
}

//! 每个IO线程中运行
void WindowsGameServer::CloseShutdownConnections() {
    YLOG_TRACE("In TcpServer::CloseShutdownConnections, 有 %zu 个shutdown连接", m_ShutdownConnections.size());

    std::vector<TcpConnectionPtr> shutdownConnections;
    {
        std::lock_guard lg{m_ShutdownConnectionsMutex};
        m_ShutdownConnections.swap(shutdownConnections);
    }

    for (const auto & conn: shutdownConnections) {
        YLOG_TRACE("In TcpServer::CloseShutdownConnections, close shutdown socket<%d>", conn->GetSocketFD());
        conn->Close();
    }
}



}