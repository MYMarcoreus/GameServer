#pragma once
#include "Socket.h"
#include "IOChannel.h"
#include <functional>

namespace yy::net {

class EventLoop;

///@brief 和TcpConnetion平级的类，都归TcpServer管理
class Acceptor {
public:
    using NewConnectionCallback = std::function<void (SocketApiWrapper::socket_t sockfd, IPAddressPtr addr)>;

    ///@param loop
    ///@param socketType 监听的套接字种类
    ///@param listenAddr 监听的套接字ip和端口
    ///@param reusePort 是否进行端口复用
    Acceptor(EventLoop * loop, Socket::Type socketType, const IPAddressPtr& listenAddr, bool reusePort);

    ~Acceptor();

    ///@brief 设置新连接到来时的回调函数
    void SetNewConnectionCallback(const NewConnectionCallback& cb) { m_NewConnectionCallback = cb; }

    ///@brief 设置套接字的监听回调函数为HandleAccept，并开始监听套接字
    void StartListen();

    void StopListen();

    bool IsListening() const { return m_IsListening; }

    IPAddressPtr GetListenAddr() const { return m_ListenAddr; }
private:
    ///@brief 接受新连接，并执行m_NewConnectionCallback
    void HandleAcceptAll();

    void StartListenInLoop();

private:
    EventLoop *             m_AcceptorLoop;
    Socket                  m_AcceptSocket;
    IOChannel               m_AcceptChannel;
    NewConnectionCallback   m_NewConnectionCallback;
    bool                    m_IsListening;
    IPAddressPtr            m_ListenAddr;
};

} // yy::net

