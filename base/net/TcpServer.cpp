#include "TcpServer.h"
#include "Acceptor.h"
#include "EventLoopThreadPool.h"
#include "EventLoop.h"
#include "ConfigManager.h"
#include "TcpConnection.h"
#include "log.h"
#include "SocketApiWrapper.h"

namespace yy::net {


TcpServer::TcpServer(EventLoop *acceptorLoop, IPAddress::ptr listenAddr, bool reusePort) noexcept
    : m_AcceptorLoop(acceptorLoop),
      m_Acceptor( new Acceptor(m_AcceptorLoop, Socket::Type::TCP, listenAddr, reusePort) ),
      m_IOThreadPool( new EventLoopThreadPool(acceptorLoop) ),
      m_AppConfigVar{config::g_app_config}
{
    m_AcceptorLoop->SetCloseSocketsCallback(m_CloseSocketsCallback);
    InitLog();
}

TcpServer::~TcpServer() {
    m_AcceptorLoop->AssertInLoopingThread();

    for(auto & p: m_ConnectionMap) {
        auto conn = p.second;
        conn->GetLoop()->RunCallbackInLoop([conn](){ conn->ConnectionDestroyed(); } );
        p.second.reset();
    }

    m_IsStarted = false;
}

void TcpServer::Start(int threadNum, F_ThreadInitCallback cb) {
    if(!m_IsStarted.exchange(true)) {
        m_IOThreadPool->Start(threadNum, nullptr, cb);
        m_Acceptor->SetNewConnectionCallback(std::bind(&TcpServer::HandleNewConnection, this, _1, _2));
        m_Acceptor->StartListen();
    }
}


void TcpServer::HandleNewConnection(SocketApiWrapper::socket_t sockfd, IPAddressPtr peerAddr) {
    m_AcceptorLoop->AssertInLoopingThread();

    EventLoop * ioLoop = m_IOThreadPool->GetNextLoop();
    IPAddressPtr localAddr = SocketApiWrapper::GetLocalAddr(sockfd);

    //! 以"连接时间:连接编号"作为连接的唯一标记，相同连接时间的连接编号一定不同
    auto name = std::format("{}:{}", Timestamp::Now().GetMircoSecondSinceEpoch().count(), m_NextConnID++);

    TcpConnectionPtr conn = std::make_shared<TcpConnection>(
            name,
            ioLoop,
            sockfd,
            localAddr,
            peerAddr
    );
    m_ConnectionMap[name] = conn;



    conn->SetConnectionEstablishedCallback(m_ConnectionEstablishedCallback);
    // conn->SetConnectionDestroyedCallback(m_ConnectionDestroyedCallback);
    conn->SetMessageCallback(m_MessageCallback);
    conn->SetConnectionWriteCompleteCallback(m_ConnectionWriteCompleteCallback);
    conn->SetConnectionCloseCallback(std::bind(&TcpServer::RemoveConnection, this, _1));
    conn->SetConnectionShutdownCallback(m_ConnectionShutdownCallback);
    conn->GetLoop()->RunCallbackInLoop([conn](){ conn->ConnectionEstablished(); });

    YLOG_INFO("In TcpServer::HandleNewConnection<{}:{}>，PeerAddr<{},{}>", conn->GetSocketFD(), conn->GetName().c_str(),
              conn->GetPeerAddr()->GetIPStr().c_str(), conn->GetPeerAddr()->GetPort());

    m_NumConnect++;
}

void TcpServer::RemoveConnection(const TcpConnectionPtr &conn) {
    m_AcceptorLoop->RunCallbackInLoop( [this, conn](){ this->RemoveConnectionInLoop(conn); });
}

void TcpServer::RemoveConnectionInLoop(TcpConnectionPtr conn) {
    m_AcceptorLoop->AssertInLoopingThread();

    m_ConnectionMap.erase(conn->GetName());
    m_NumConnect--;

    conn->GetLoop()->EnqueueCallbackInLoop([conn](){ conn->ConnectionDestroyed(); });
}



void TcpServer::InitLog() {
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
