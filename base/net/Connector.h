#ifndef LINUXGAMESERVER_CONNECTOR_H
#define LINUXGAMESERVER_CONNECTOR_H

#include "IPAddress.h"
#include "net_definations.h"
#include <chrono>

namespace yy::net {

class EventLoop;
class Channel;


///@brief 对标Acceptor，实现了非阻塞connect
class Connector {
    enum E_ConnectionState {
        eDisconnected,
        eConnecting,
        eConnected
    };

    using F_NewConnectionCallback = std::function<void (SocketApiWrapper::socket_t sockfd)>;
    using F_ConnectFailedCallback = std::function<void ()>;
public:

    Connector(EventLoop *loop, const IPAddressPtr & serverAddr);
    ~Connector();

    ///@brief 开始连接，直到延迟时间超过最大值
    void Start();

    ///@brief 重启延迟时间，再次开始连接
    void Restart();

    ///@brief 停止正在连接的操作
    void Stop();

    void SetNewConnectionCallback(const F_NewConnectionCallback &newConnectionCallback) {
        m_NewConnectionCallback = newConnectionCallback;
    }

    void SetConnectFailedCallback(const F_ConnectFailedCallback &connectFailedCallback) {
        m_ConnectFailedCallback = connectFailedCallback;
    }


private:
    void SetState(E_ConnectionState state) { m_State = state; }

    //! 非阻塞connect后根据错误码选择Connecting（用Channel监听写/错误事件以等待连接完成）或Retry（等待一定延时后再连接）
    void StartInLoop();
    void RestartInLoop();
    void StopInLoop();


    //! 递增RetryDelay，再次开始连接
    void Retry(SocketApiWrapper::socket_t sockfd);

    //! 非阻塞connect已返回，监听sockfd的写/错误事件
    void Connecting(SocketApiWrapper::socket_t sockfd);

    ///@brief 连接成功时，会触发套接字的可写事件
    void HandleWrite();

    ///@brief 连接失败时，会触发套接字的错误事件
    void HandleError();

    //! 连接成功时，删除一次性的channel，是因为channel已完成它监听非阻塞连接成功/失败的任务；
    //! 连接失败时，删除一次性的channel，是为了下次新建一个channel重新连接
    int RemoveAndResetChannel();

private:
    EventLoop *                  m_Loop;
    IPAddressPtr                 m_ServerAddr;
    std::unique_ptr<Channel>     m_Channel; //! 在connect中，sockfd是一次性的，所以其对应的channel也是一次性的
    E_ConnectionState            m_State;
    bool                         m_IsStarted;
    F_NewConnectionCallback      m_NewConnectionCallback;
    F_ConnectFailedCallback      m_ConnectFailedCallback;
    Milliseconds                 m_RetryDelay; //!
    TimerID                      m_NextRetryTimerID;

    static constexpr Milliseconds kInitRetryDelay = 500ms;
    static constexpr Milliseconds kMaxRetryDelay = 30s;
};


}

#endif //LINUXGAMESERVER_CONNECTOR_H

