#include "TcpClient.h"
#include "TcpConnection.h"
#include "Connector.h"
#include "IOChannel.h"
#include "Socket.h"
#include "EventLoop.h"
#include "log.h"
#include "SignalManager.h"
#include "SocketApiWrapper.h"


namespace yy::net {


TcpClient::TcpClient(EventLoop *loop, IPAddressPtr serverAddr,
const int32_t send_bytes_one, const int32_t send_bytes_max,
const int32_t recv_bytes_one, const int32_t recv_bytes_max, const uint8_t xor_code) :
    m_send_bytes_one(send_bytes_one),
    m_send_bytes_max(send_bytes_max),
    m_recv_bytes_one(recv_bytes_one),
    m_recv_bytes_max(recv_bytes_max),
    m_xorCode(xor_code),
    m_Loop(loop),
    m_Connection{},
    m_Connector(std::make_shared<Connector>(loop, serverAddr)),
    m_CanAutoRetry{true},
    m_IsStarted{false},
    m_NextConnID{0},
    m_ServerAddr{serverAddr}
{
    m_Connector->SetNewConnectionCallback( [this](const SocketApiWrapper::socket_t sockfd){ this->NewConnection(sockfd); } );
    m_Connector->SetConnectFailedCallback( [this]() { YLOG_WARN("coonect to <{}:{}>", this->m_ServerAddr->GetIPStr().c_str(), m_ServerAddr->GetPort()) } );

#ifdef ____LINUX
    util::SignalManager::set_signal_ignore(SIGPIPE);
#endif
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

    // auto name = std::format("{}:{}", Timestamp::Now().GetMircoSecondSinceEpoch().count(), m_NextConnID++);
    auto connid = m_NextConnID++;

    TcpConnectionPtr conn = std::make_shared<TcpConnection>(
            connid,
            m_Loop,
            sockfd,
            localAddr,
            peerAddr,
            m_send_bytes_one,
            m_send_bytes_max,
            m_recv_bytes_one,
            m_recv_bytes_max,
            m_xorCode
    );

    {
        std::lock_guard lg{m_ConnectionMutex};
        m_Connection = conn;
    }
    conn->SetConnectionEstablishedCallback(m_ConnectionEstablishedCallback);
    conn->SetMessageCallback(m_MessageCallback);
    conn->SetConnectionWriteCompleteCallback(m_ConnectionWriteCompleteCallback);
    conn->SetConnectionCloseCallback([this](const TcpConnectionPtr& tcpconn){ this->RemoveConnection(tcpconn); });

    conn->GetIOLoop()->RunCallbackInLoop([conn](){ conn->ConnectionEstablished(); });
}

void TcpClient::RemoveConnection(TcpConnectionPtr conn) {
    {
        std::lock_guard lg{m_ConnectionMutex};
        m_Connection.reset();
    }
    conn->GetIOLoop()->EnqueueCallbackInLoop([conn](){conn->ConnectionDestroyed();});

    if(m_CanAutoRetry and m_IsStarted)
        m_Connector->Restart();
}



}
