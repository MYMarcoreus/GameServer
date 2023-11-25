#include "TcpClient.h"
#include "TcpConnection.h"
#include "Connector.h"
#include "Channel.h"
#include "Socket.h"
#include "EventLoop.h"
#include "log.h"
#include "SocketApiWrapper.h"


namespace yy::net {


TcpClient::TcpClient(EventLoop *loop, IPAddressPtr serverAddr)
    : m_IsStarted{false},
      m_CanAutoRetry{true},
      m_Loop(loop),
      m_Connection(),
      m_Connector(std::make_shared<Connector>(loop, serverAddr)),
      m_ServerAddr{serverAddr}
{
    InitLog();
    m_Connector->SetNewConnectionCallback( std::bind(&TcpClient::NewConnection, this, _1) );
    m_Connector->SetConnectFailedCallback( [this]() { YLOG_WARN("coonect to <{}:{}>", this->m_ServerAddr->GetIPStr().c_str(), m_ServerAddr->GetPort()) } );
}

TcpClient::~TcpClient() {
    TcpConnectionPtr conn;
    bool isUnique = false;
    {
        std::lock_guard lg{m_ConnectionMutex};
        if(m_Connection)
            isUnique = m_Connection.use_count() == 1;
        conn = m_Connection;
    }

    if(conn) {
        m_Loop->EnqueueCallbackInLoop([conn](){ conn->ConnectionDestroyed(); });
        if (isUnique) {
            conn->Shutdown();
        }
    }
}

void TcpClient::Connect() {
    m_IsStarted = true;
    m_Connector->Start();
}

void TcpClient::Disconnect() {
    m_IsStarted = false;
    {
        std::lock_guard lg{m_ConnectionMutex};
        if(m_Connection)
            m_Connection->Shutdown();
    }
}

void TcpClient::StopConnecting() {
    m_IsStarted = false;
    m_Connector->Stop();
}

void TcpClient::NewConnection(SocketApiWrapper::socket_t sockfd) {
    IPAddressPtr localAddr = SocketApiWrapper::GetLocalAddr(sockfd);
    IPAddressPtr peerAddr = SocketApiWrapper::GetPeerAddr(sockfd);

    auto name = std::format("{}:{}", Timestamp::Now().GetMircoSecondSinceEpoch().count(), m_NextConnID++);

    TcpConnectionPtr conn = std::make_shared<TcpConnection>(
            name,
            m_Loop,
            sockfd,
            localAddr,
            peerAddr
    );
    {
        std::lock_guard lg{m_ConnectionMutex};
        m_Connection = conn;
    }
    conn->SetConnectionEstablishedCallback(m_ConnectionEstablishedCallback);
    conn->SetMessageCallback(m_MessageCallback);
    conn->SetConnectionWriteCompleteCallback(m_ConnectionWriteCompleteCallback);
    conn->SetConnectionCloseCallback(std::bind(&TcpClient::RemoveConnection, this, _1));
    conn->ConnectionEstablished();
}

void TcpClient::RemoveConnection(TcpConnectionPtr conn) {
    {
        std::lock_guard lg{m_ConnectionMutex};
        m_Connection.reset();
    }
    conn->GetLoop()->EnqueueCallbackInLoop([conn](){conn->ConnectionDestroyed();});

    if(m_CanAutoRetry and m_IsStarted)
        m_Connector->Restart();
}

void TcpClient::InitLog() {
    yy::Ylog::LoggerManager::getInstance().ReadConfigs();
// #ifdef ____DEBUG
//     yy::Ylog::LoggerManager::getInstance().getLogger()->setLevel(yy::Ylog::LogLevel::eTRACE);
// #else
//     yy::Ylog::LoggerManager::getInstance().getLogger()->setLevel(yy::Ylog::LogLevel::eINFO);
// #endif
//     yy::Ylog::LogAppender::ptr appender{new yy::Ylog::StdoutLogApeender{"[%t][%l]%c%n"}};
//     yy::Ylog::LoggerManager::getInstance().getLogger()->addAppender(appender);
}


}
