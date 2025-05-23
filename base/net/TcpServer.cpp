#include "TcpServer.h"
#include "Acceptor.h"
#include "EventLoopThreadPool.h"
#include "EventLoop.h"
#include "ConfigManager.h"
#include "TcpConnection.h"
#include "log.h"
#include "SocketApiWrapper.h"
#include "util_functions.h"
#include "FullDuplexPipe.h"

namespace yy::net {

#ifdef ____LINUX

class SignalManager {
public:
    SignalManager(EventLoop * loop, std::function<void()> handler)
        : loop_{loop}, channel_(std::make_unique<IOChannel>(loop_, SignalManager::pipe_.sideR(), "Wakeup Eventfd Channel"))
    {
        // 屏蔽SIGPIPE：当服务器进程向已收到RST的用户套接字执行写操作时，内核会向进程发送SIGPIPE信号来结束进程
        util::set_signal_ignore(SIGPIPE);

        channel_->SetReadCallback(handler);
        channel_->EnableReading();

        // 设置三个信号处理函数：该处理函数将信号通过管道传送
        yy::util::set_signal_handler(SIGALRM, WritePipe);
        yy::util::set_signal_handler(SIGINT, WritePipe);/* Ctrl+c */
        yy::util::set_signal_handler(SIGTERM, WritePipe);// kill <pid>
    }


    static void WritePipe(int sig) {
        //! 向唤醒事件文件描述符进行写，以触发其eoll事件
        int msg = sig;
        pipe_.Write((const char *)&msg, 1);
    }

    static std::string ReadPipe() {
        //! 向唤醒事件文件描述符进行写，以触发其epoll事件
        static std::string sigs;
        sigs.assign(128, 0);
        auto nSig = pipe_.Read(sigs.data(), sigs.size());
        return sigs;
    }

    static FullDuplexPipe pipe_;

private:
    EventLoop * loop_;
    std::unique_ptr<IOChannel> channel_;
};

FullDuplexPipe SignalManager::pipe_{};


#endif






TcpServer::TcpServer(EventLoop *acceptorLoop, IPAddress::ptr listenAddr, bool reusePort) noexcept
    : m_AcceptorLoop(acceptorLoop)
    , m_Acceptor(std::make_unique<Acceptor>(m_AcceptorLoop, Socket::Type::TCP, listenAddr, reusePort))
    , m_IOThreadPool(std::make_unique<EventLoopThreadPool>(acceptorLoop))
    , m_AppConfigVar{config::g_app_config}
#ifdef ____LINUX
    , m_SignalManager{std::make_unique<SignalManager>(acceptorLoop, [this](){ this->HandleSignal(); })}
#endif
{
#ifdef ____LINUX
    util::set_signal_ignore(SIGPIPE);
#endif
}

TcpServer::~TcpServer() {
    m_AcceptorLoop->AssertInLoopingThread(__FILE__, __LINE__);

    for(auto & p: m_ConnectionMap) {
        auto conn = p.second;
        conn->GetIOLoop()->RunCallbackInLoop([conn](){ conn->ConnectionDestroyed(); } );
        conn.reset();
    }

    m_IsStarted = false;
}

void TcpServer::Start(int ioThreadNum, Milliseconds ioWaitTimeout, F_ThreadInitCallback cb) {
    if(!m_IsStarted.exchange(true)) {
        m_IOThreadPool->Start(ioThreadNum, ioWaitTimeout, cb);
        m_Acceptor->SetNewConnectionCallback(std::bind(&TcpServer::HandleNewConnection, this, _1, _2));
        m_Acceptor->StartListen();
    }
}


void TcpServer::Stop() {
    m_Acceptor->StopListen();
}


void TcpServer::HandleNewConnection(SocketApiWrapper::socket_t sockfd, IPAddressPtr peerAddr) {
    m_AcceptorLoop->AssertInLoopingThread(__FILE__, __LINE__);

    EventLoop * ioLoop = m_IOThreadPool->GetNextLoop();
    IPAddressPtr localAddr = SocketApiWrapper::GetLocalAddr(sockfd);

    //! 以"连接时间:连接编号"作为连接的唯一标记，相同连接时间的连接编号一定不同
    auto name = std::format("{:020}-{:011}", Timestamp::Now().GetMircoSecondSinceEpoch().count(), m_NextConnID++);

    TcpConnectionPtr conn = std::make_shared<TcpConnection>(
            name,
            ioLoop,
            sockfd,
            localAddr,
            peerAddr
    );
    m_ConnectionMap[name] = conn;

    // YLOG_INFO("连接[{}], {}", name, name.size());

    //! 传递上层的回调
    conn->SetConnectionEstablishedCallback(m_ConnectionEstablishedCallback);
    conn->SetConnectionDestroyedCallback(m_ConnectionDestroyedCallback);
    conn->SetMessageCallback(m_MessageCallback);
    conn->SetConnectionWriteCompleteCallback(m_ConnectionWriteCompleteCallback);
    conn->SetConnectionCloseCallback(std::bind(&TcpServer::RemoveConnection, this, _1));
    conn->SetConnectionShutdownCallback(m_ConnectionShutdownCallback);

    //!
    conn->GetIOLoop()->RunCallbackInLoop([conn](){ conn->ConnectionEstablished(); });

    YLOG_INFO("In TcpServer::HandleNewConnection<{}:{}>，PeerAddr<{},{}>", conn->GetSocketFD(), conn->GetName().c_str(),
              conn->GetPeerAddr()->GetIPStr().c_str(), conn->GetPeerAddr()->GetPortStr().c_str());

    m_NumConnect++;
}

void TcpServer::RemoveConnection(const TcpConnectionPtr &conn) {
    //! 该函数在 TcpConnection::ioLoop中执行
    m_AcceptorLoop->RunCallbackInLoop( [this, conn](){ this->RemoveConnectionInLoop(conn); });
}

void TcpServer::RemoveConnectionInLoop(TcpConnectionPtr conn) {
    m_AcceptorLoop->AssertInLoopingThread(__FILE__, __LINE__);

    m_ConnectionMap.erase(conn->GetName());
    m_NumConnect--;

    conn->GetIOLoop()->EnqueueCallbackInLoop([conn](){ conn->ConnectionDestroyed(); });
}





void TcpServer::SetCloseSocketsCallback(F_CloseShutdownConnectionsCallback cb) {
    m_AcceptorLoop->SetCloseSocketsCallback(cb);
}

void TcpServer::HandleSignal() {
#ifdef ____LINUX
    auto sigs = SignalManager::ReadPipe();

    for(int i = 0 ; i < sigs.size() ; ++i) {
        switch((int)sigs[i]) {
            // 定时器
            case SIGALRM: {
                YLOG_WARN("收到SIGALRM信号！")
                break;
            }
            // case SIGQUIT: /* Ctrl+\ */
            case SIGINT:  /* Ctrl+C */
            case SIGTERM: // kill <pid>
            // case SIGKILL: // kill -9 <pid>
            {
                YLOG_WARN("收到{}信号，结束服务器进程！", strsignal(sigs[i]))
                this->Stop();
                break;
            }
            case 0:
                break;
            default: {
                YLOG_WARN("收到其它信号！")
                break;
            }
        }
    }
#endif
}




}

