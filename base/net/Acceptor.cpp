#include "Acceptor.h"
#include "EventLoop.h"
#include "IPAddress.h"
#include "log.h"
#include "status/Status.h"
#include "SocketApiWrapper.h"

namespace yy::net {

using namespace yy::util;

Acceptor::Acceptor(EventLoop *loop, Socket::Type socketType, const IPAddressPtr listenAddr, bool reusePort)
        : m_AcceptorLoop(loop),
          m_IsListening(false),
          m_AcceptSocket(socketType, (Socket::Family)listenAddr->GetFamily(), true), //! 非阻塞监听套接字
          m_AcceptChannel(loop, m_AcceptSocket.GetFD(), "Acceptor Channel"),
          m_ListenAddr(listenAddr)
{
    //! 初始化监听套接字（尚未开始监听）
    m_AcceptSocket.SetOpt_ReuseAddr(true);
    //m_AcceptSocket.SetOpt_ReusePort(reusePort);
    m_AcceptSocket.SetOpt_Linger(true);
    m_AcceptSocket.Bind(listenAddr);
}

Acceptor::~Acceptor() {
    // 调用Channel和Socket的析构函数
    m_AcceptChannel.DisableAllEvent();
    m_AcceptChannel.RemoveFromLoop();
}


void Acceptor::StartListen() {
    m_AcceptorLoop->RunCallbackInLoop([this](){ this->StartListenInLoop(); });
}

void Acceptor::HandleAccept() {
    m_AcceptorLoop->AssertInLoopingThread();

    IPAddressPtr outPeerAddr = nullptr;
    switch (m_AcceptSocket.GetFamily()) {
        case Socket::Family::IPv4:
            outPeerAddr = std::make_shared<IPv4Address>();
            break;
        case Socket::Family::IPv6:
            outPeerAddr = std::make_shared<IPv6Address>();
            break;
        default:
            throw std::invalid_argument("wrong socket family of accept socket");
    }

    SocketApiWrapper::socket_t connfd = m_AcceptSocket.Accept(outPeerAddr, true); //! 非阻塞连接套接字

    YLOG_DEBUG("In Acceptor::HandleAccept，套接字<{}>被Accept", connfd)
    if(m_NewConnectionCallback) {
        m_NewConnectionCallback(connfd, outPeerAddr); // TcpServer定义
    } else {
        SocketApiWrapper::close(connfd);
    }
}

void Acceptor::StartListenInLoop() {
    m_AcceptorLoop->AssertInLoopingThread();

    m_IsListening = true;
    m_AcceptChannel.SetReadCallback([this](){ this->HandleAccept(); });
    m_AcceptChannel.EnableReading();
    m_AcceptSocket.Listen();
    YLOG_INFO("线程<{}>开始监听，监听地址为：<{}:{}>，监听套接字为{}", GetStrThreadID(),
              m_ListenAddr->GetIPStr().c_str(), m_ListenAddr->GetPort(), m_AcceptSocket.GetFD());

}

void Acceptor::StopListen() {
    m_AcceptorLoop->QuitLoop();
    m_IsListening = false;
}


} // yy::net

