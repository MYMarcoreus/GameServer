#include "Acceptor.h"
#include "EventLoop.h"
#include "IPAddress.h"
#include "log.h"
#include "status/Status.h"
#include "SocketApiWrapper.h"

namespace yy::net {

using namespace yy::util;

Acceptor::Acceptor(EventLoop *loop, const Socket::Type socketType, const IPAddressPtr& listenAddr, const bool reusePort)
        : m_AcceptorLoop(loop),
          m_AcceptSocket(socketType, static_cast<Socket::Family>(listenAddr->GetFamily()), true),
          m_AcceptChannel(loop, m_AcceptSocket.GetFD(), "Acceptor Channel"), //! 非阻塞监听套接字
          m_IsListening(false),
          m_ListenAddr(listenAddr)
{
    //! 初始化监听套接字（尚未开始监听）
    m_AcceptSocket.SetOpt_ReuseAddr(reusePort);
    //m_AcceptSocket.SetOpt_ReusePort(reusePort);
    m_AcceptSocket.SetOpt_Linger(true);
    m_AcceptSocket.Bind(listenAddr);
}

Acceptor::~Acceptor() {
    // 调用Channel和Socket的析构函数
    m_AcceptChannel.ResetAndRemoveFromPoller();
}


void Acceptor::StartListen() {
    m_AcceptorLoop->RunCallbackInLoop([this](){ this->StartListenInLoop(); });
}

void Acceptor::StartListenInLoop() {
    m_AcceptorLoop->AssertInLoopingThread();

    m_IsListening = true;
    m_AcceptChannel.SetReadCallback([this](){ this->HandleAcceptAll(); });
    m_AcceptChannel.EnableReading();
    m_AcceptSocket.Listen();
    YLOG_INFO("线程<{}>开始监听，监听地址为：<{}:{}>，监听套接字为{}", GetStrThreadID(),
              m_ListenAddr->GetIPStr().c_str(), m_ListenAddr->GetPort(), m_AcceptSocket.GetFD());
}

void Acceptor::HandleAcceptAll() {
    m_AcceptorLoop->AssertInLoopingThread();

    const auto allConnfd = m_AcceptSocket.AcceptAll(true); //! 非阻塞连接套接字

    for (const auto & [conn_fd, coon_addr]: allConnfd) {
        YLOG_DEBUG("In Acceptor::HandleAccept，套接字<{}>被Accept", conn_fd)
        if(m_NewConnectionCallback) {
            m_NewConnectionCallback(conn_fd, coon_addr); // TcpServer::HandleNewConnection
        } else {
            SocketApiWrapper::close(conn_fd);
        }
    }

}

void Acceptor::StopListen() {
    m_AcceptorLoop->QuitLoop();
    m_IsListening = false;
}


} // yy::net

