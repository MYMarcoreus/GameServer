#include "TcpServer.h"

#include <ranges>

#include "Acceptor.h"
#include "EventLoopThreadPool.h"
#include "EventLoop.h"
#include "TcpConnection.h"
#include "log.h"
#include "SocketApiWrapper.h"
#include "util_functions.h"
#include "SignalManager.h"



namespace yy::net {

TcpServer::TcpServer(EventLoop *acceptorLoop, IPAddress::ptr listenAddr, bool reusePort,
const int32_t send_bytes_one, const int32_t send_bytes_max,
const int32_t recv_bytes_one, const int32_t recv_bytes_max, const uint8_t xor_code) noexcept:
    m_send_bytes_one(send_bytes_one),
    m_send_bytes_max(send_bytes_max),
    m_recv_bytes_one(recv_bytes_one),
    m_recv_bytes_max(recv_bytes_max),
    m_xorCode(xor_code),
    m_AcceptorLoop(acceptorLoop) ,
    m_Acceptor(std::make_unique<Acceptor>(m_AcceptorLoop, Socket::Type::TCP, listenAddr, reusePort)),
    m_IOThreadPool(std::make_unique<EventLoopThreadPool>(acceptorLoop))
#ifdef ____LINUX
     ,m_SignalManager(std::make_unique<SignalManager>(acceptorLoop, [this](){ this->HandleSignal(); }))
#endif
{
#ifdef ____LINUX
    SignalManager::set_signal_ignore(SIGPIPE);
#endif
}

TcpServer::~TcpServer() {
    m_AcceptorLoop->AssertInLoopingThread();

    for(auto& conn : m_ConnectionMap | std::views::values) {
        conn->GetIOLoop()->RunCallbackInLoop([conn](){ conn->ConnectionDestroyed(); } );
        conn.reset();
    }

    m_IsStarted = false;
}

void TcpServer::Start(const int ioThreadNum, const Milliseconds ioWaitTimeout, const F_ThreadInitCallback& cb) {
    if(!m_IsStarted.exchange(true)) {
        m_IOThreadPool->Start(ioThreadNum, ioWaitTimeout, cb);
        m_Acceptor->SetNewConnectionCallback([this](const SocketApiWrapper::socket_t sockfd, const IPAddressPtr& peerAddr){return this->HandleNewConnection(sockfd, peerAddr);} );
        m_Acceptor->StartListen();
    }
}


void TcpServer::Stop() const
{
    m_Acceptor->StopListen();
}


auto TcpServer::GetListenAddr() const -> IPAddressPtr
{
    return m_Acceptor->GetListenAddr();
}

void TcpServer::HandleNewConnection(SocketApiWrapper::socket_t sockfd, IPAddressPtr peerAddr) {
    m_AcceptorLoop->AssertInLoopingThread();

    EventLoop * ioLoop = m_IOThreadPool->GetNextLoop();
    IPAddressPtr localAddr = SocketApiWrapper::GetLocalAddr(sockfd);

    //! 以"连接时间:连接编号"作为连接的唯一标记，相同连接时间的连接编号一定不同
    // auto name = std::format("{:020}-{:011}", Timestamp::Now().GetMircoSecondSinceEpoch().count(), m_NextConnID++);
    auto connid = m_NextConnID++;

    TcpConnectionPtr conn = std::make_shared<TcpConnection>(
            connid,
            ioLoop,
            sockfd,
            localAddr,
            peerAddr,
            m_send_bytes_one,
            m_send_bytes_max,
            m_recv_bytes_one,
            m_recv_bytes_max,
            m_xorCode
    );
    m_ConnectionMap[conn->GetConnID()] = conn;

    // YLOG_INFO("连接[{}], {}", name, name.size());

    //! 传递上层的回调
    conn->SetConnectionEstablishedCallback(m_ConnectionEstablishedCallback);
    conn->SetConnectionDestroyedCallback(m_ConnectionDestroyedCallback);
    conn->SetMessageCallback(m_MessageCallback);
    conn->SetConnectionWriteCompleteCallback(m_ConnectionWriteCompleteCallback);
    conn->SetConnectionCloseCallback([this](const TcpConnectionPtr & conn_cb){ return this->RemoveConnection(conn_cb); });
    conn->SetConnectionShutdownCallback(m_ConnectionShutdownCallback);

    //!
    conn->GetIOLoop()->RunCallbackInLoop([conn](){ conn->ConnectionEstablished(); });

    YLOG_INFO("In TcpServer::HandleNewConnection<{}:{}>，PeerAddr<{},{}>", conn->GetSocketFD(), conn->GetConnID(),
              conn->GetPeerAddr()->GetIPStr().c_str(), conn->GetPeerAddr()->GetPortStr().c_str());

    ++m_NumConnect;
}

void TcpServer::RemoveConnection(const TcpConnectionPtr &conn) {
    //! 该函数在 TcpConnection::ioLoop中执行
    m_AcceptorLoop->RunCallbackInLoop( [this, conn](){ this->RemoveConnectionInLoop(conn); });
}

void TcpServer::RemoveConnectionInLoop(TcpConnectionPtr conn) {
    m_AcceptorLoop->AssertInLoopingThread();

    m_ConnectionMap.erase(conn->GetConnID());
    --m_NumConnect;

    conn->GetIOLoop()->EnqueueCallbackInLoop([conn](){ conn->ConnectionDestroyed(); });
}





void TcpServer::SetCloseSocketsCallback(const F_CloseShutdownConnectionsCallback& cb) {
    m_AcceptorLoop->SetCloseSocketsCallback(cb);
}

void TcpServer::HandleSignal() {
#ifdef ____LINUX
    auto sigs = SignalManager::ReadPipe();

    for(int i = 0 ; i < sigs.size() ; ++i) {
        switch(sigs[i]) {
            // 定时器
            case SIGALRM: {
                YLOG_WARN("收到SIGALRM信号！")
                break;
            }
            // case SIGQUIT: /* Ctrl+\ */
            case SIGINT:  /* Ctrl+C */
            case SIGTERM: // kill <pid>
            case SIGKILL: // kill -9 <pid>
            {
                auto info = std::format("收到{}信号，结束服务器进程！", strsignal(sigs[i]));
                std::cerr << info << std::endl;
                YLOG_WARN("{}", info)

                this->Stop();
                break;
            }
            case 0:
                break;
            default: {
                auto info = std::format("收到{}信号！", strsignal(sigs[i]));
                std::cerr << info << std::endl;
                YLOG_WARN("{}", info)
                break;
            }
        }
    }
#endif
}




}

