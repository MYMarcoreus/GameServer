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


TcpClient::TcpClient(EventLoop *loop,
const int32_t send_bytes_one, const int32_t send_bytes_max,
const int32_t recv_bytes_one, const int32_t recv_bytes_max,
const uint8_t xor_code, IPAddressPtr serverAddr) :
    m_send_bytes_one(send_bytes_one),
    m_send_bytes_max(send_bytes_max),
    m_recv_bytes_one(recv_bytes_one),
    m_recv_bytes_max(recv_bytes_max),
    m_xorCode(xor_code),
    m_Loop(loop),
    m_Connection{nullptr},
    m_Connector(serverAddr ? std::make_shared<Connector>(loop, serverAddr) : nullptr),
    m_CanAutoRetry{true},
    m_IsStarted{false},
    m_NextConnID{0},
    m_ServerAddr{serverAddr}
{
    assert(m_Loop != nullptr);

    if (m_Connector) {
        m_Connector->SetNewConnectionCallback( [this](const SocketApiWrapper::socket_t sockfd){ this->NewConnection(sockfd); } );
        m_Connector->SetConnectFailedCallback( [this]() { YLOG_WARN("coonect to <{}:{}>", this->m_ServerAddr->GetIPStr().c_str(), m_ServerAddr->GetPort()) } );
    }
#ifdef ____LINUX
    util::SignalManager::set_signal_ignore(SIGPIPE);
#endif
}


TcpClient::~TcpClient() {
    TcpConnectionPtr conn;
    bool isUnique = false;
    {
        util::ReadLockGuard lg{m_ConnectionMutex};
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

bool TcpClient::Connect(const IPAddressPtr& server_addr) {
    m_IsStarted = true;

    if (server_addr) {
        m_ServerAddr = server_addr;
        m_Connector = std::make_shared<Connector>(m_Loop, m_ServerAddr);
        m_Connector->SetNewConnectionCallback( [this](const SocketApiWrapper::socket_t sockfd){ this->NewConnection(sockfd); } );
        m_Connector->SetConnectFailedCallback( [this]() { YLOG_WARN("coonect to <{}:{}>", this->m_ServerAddr->GetIPStr().c_str(), m_ServerAddr->GetPort()) } );
    }
    if (m_ServerAddr == nullptr) {
        YLOG_ERROR("TcpClient::Connect empty m_ServerAddr!")
        return false;
    }

    m_Connector->Start();
    return true;
}

bool TcpClient::ConnectSync(const IPAddressPtr& server_addr) {
    m_IsStarted = true;

    if (server_addr) {
        m_ServerAddr = server_addr;
        m_Connector = std::make_shared<Connector>(m_Loop, m_ServerAddr);
        m_Connector->SetNewConnectionCallback( [this](const SocketApiWrapper::socket_t sockfd){ this->NewConnection(sockfd); } );
        m_Connector->SetConnectFailedCallback( [this]() { YLOG_WARN("coonect to <{}:{}>", this->m_ServerAddr->GetIPStr().c_str(), m_ServerAddr->GetPort()) } );
    }
    if (m_ServerAddr == nullptr) {
        YLOG_ERROR("TcpClient::Connect empty m_ServerAddr!")
        return false;
    }

    m_Connector->Start();

    // 阻塞直到连接成功
    while (m_IsConnected.load(std::memory_order::acquire) == false) {
        m_IsConnected.wait(false);
    }

    return true;
}

void TcpClient::Disconnect() {
    m_IsStarted = false;
    {
        util::WriteLockGuard lg{m_ConnectionMutex};
        if(m_Connection)
            m_Connection->Shutdown();
    }
}

void TcpClient::StopConnecting() {
    m_IsStarted = false;
    m_Connector->Stop();
}

TcpConnectionPtr TcpClient::GetConnection()
{
    util::ReadLockGuard lg{m_ConnectionMutex};
    return m_Connection;
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

    conn->SetConnectionEstablishedCallback(m_ConnectionEstablishedCallback);
    conn->SetMessageCallback(m_MessageCallback);
    conn->SetConnectionWriteCompleteCallback(m_ConnectionWriteCompleteCallback);
    conn->SetConnectionCloseCallback([this](const TcpConnectionPtr& tcpconn) {
        this->RemoveConnection(tcpconn);
        m_IsConnected.store(false, std::memory_order_release);
    });

    conn->GetIOLoop()->RunCallbackInLoop([this, conn]() {
        conn->ConnectionEstablished();
        m_IsConnected.store(true, std::memory_order_release);
        m_IsConnected.notify_one();
    });
    {
        util::WriteLockGuard lg{m_ConnectionMutex};
        m_Connection = conn;
    }
}

void TcpClient::RemoveConnection(TcpConnectionPtr conn) {
    {
        util::WriteLockGuard lg{m_ConnectionMutex};
        m_Connection.reset();
    }
    conn->GetIOLoop()->EnqueueCallbackInLoop([conn](){conn->ConnectionDestroyed();});

    if(m_CanAutoRetry and m_IsStarted)
        m_Connector->Restart();
}



}
