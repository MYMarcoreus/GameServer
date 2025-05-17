#include "Connector.h"
#include "IOChannel.h"
#include "EventLoop.h"
#include "log.h"
#include "ErrnoSaver.h"
#include "status/Status.h"
#include "SocketApiWrapper.h"
#include <algorithm>

namespace yy::net {


Connector::Connector(EventLoop *loop, const IPAddressPtr & serverAddr)
    : m_Loop(loop),
    m_ServerAddr(serverAddr),
    m_IsStarted{false},
    m_Channel{nullptr},
    m_State{eDisconnected},
    m_NextRetryTimerID{-1},
    m_RetryDelay(kInitRetryDelay)
{ }

void Connector::Start() {
    m_IsStarted = true;
    m_Loop->RunCallbackInLoop( [this](){ this->StartInLoop(); } );
}

void Connector::Stop() {
    m_IsStarted = false;

    //! 放入代办函数，因为要先让发生的事件（可能有连接套接字的写或错误事件）先执行完
    m_Loop->EnqueueCallbackInLoop( [this](){ this->StopInLoop(); } );
}

void Connector::Restart() {
    m_IsStarted = true;
    m_Loop->RunCallbackInLoop( [this](){ this->RestartInLoop(); } );
}

void Connector::StartInLoop() {
    m_Loop->AssertInLoopingThread(__FILE__, __LINE__);

    if(!m_IsStarted) {
        return;
    }
    //! 创建非阻塞套接字并开始非阻塞connect
    SocketApiWrapper::socket_t sockfd = SocketApiWrapper::create_or_die();
    int ret = SocketApiWrapper::connect(sockfd, m_ServerAddr);
    YLOG_INFO("In Connector::StartInLoop(), 开始连接服务器<{}:{}>", m_ServerAddr->GetIPStr().c_str(), m_ServerAddr->GetPort())

#ifdef ____WINDOWS
    auto errnoSaver = ret==0 ? 0 : GetLastError();
#endif
#ifdef ____LINUX
    auto errnoSaver = ret==0 ? 0 : errno;
#endif
    switch (errnoSaver) {
        //! 非阻塞connect立即返回，于是用Channel监听写/错误事件以等待连接完成
        case 0:
        case EINPROGRESS:
        case EINTR:
        case EISCONN:
            Connecting(sockfd);
            break;
        //! 非阻塞connect立即返回失败，等待一定延时之后再重试
        case EAGAIN:
        case EADDRINUSE:
        case EADDRNOTAVAIL:
        case ECONNREFUSED:
        case ENETUNREACH:
            //! 如果重试到最大等待时间，就停止
            if(m_RetryDelay == kMaxRetryDelay) {
                if(m_ConnectFailedCallback)
                    m_ConnectFailedCallback();
                StopInLoop();
                break;
            }

            Retry(sockfd);
            break;
        //! 连接错误！关闭连接
        case EACCES:
        case EPERM:
        case EAFNOSUPPORT:
        case EALREADY:
        case EBADF:
        case EFAULT:
        case ENOTSOCK:
            SocketApiWrapper::close(sockfd);
            YLOG_FATAL("connect error in Connector::StartInLoop: {}", util::GetErrorInfo(errnoSaver));
            break;

        default:
            SocketApiWrapper::close(sockfd);
            YLOG_FATAL("Unexpected error in Connector::StartInLoop: {}", util::GetErrorInfo(errnoSaver));
            // connectErrorCallback_();
            break;
    }
}

void Connector::RestartInLoop() {
    YLOG_TRACE("In Connector::RestartInLoop(), 重新开始连接，自动重连延时重置<{}:{}>", m_ServerAddr->GetIPStr().c_str(), m_ServerAddr->GetPort())
    SetState(eDisconnected);
    m_RetryDelay = kInitRetryDelay; //! 重置延时
    m_IsStarted = true;
    StartInLoop();
}

void Connector::StopInLoop() {
    YLOG_TRACE("In Connector::StopInLoop(), 中止连接服务器<{}:{}>", m_ServerAddr->GetIPStr().c_str(), m_ServerAddr->GetPort())
    //! 中止未连接完成的连接
    if(m_State == eConnecting) {
        if(m_NextRetryTimerID != -1)
            m_Loop->CancelTimer(m_NextRetryTimerID);
        SetState(eDisconnected);
        SocketApiWrapper::socket_t sockfd = RemoveAndResetChannel();
        SocketApiWrapper::close(sockfd);
    }
}

void Connector::HandleWrite() {
    //!
    if(m_State != eConnecting)
        return;

    SocketApiWrapper::socket_t sockfd = RemoveAndResetChannel(); //! 连接已建立，删除连接监听Channel
    int err = SocketApiWrapper::get_socket_error(sockfd);

    //! socket可写时，并非一定是套接字连接完成，也可能是发生了错误
    if(err) {
        YLOG_ERROR("In Connector::HandleWrite(), Socket Error: {}", yy::util::GetErrorInfo(err))
        Retry(sockfd);
    }
    //! 发生了自连接的情况：即客户端随机分配的端口号与服务端发生了重复
    else if(SocketApiWrapper::is_self_connect(sockfd)) {
        YLOG_ERROR("In Connector::HandleWrite(), Self Connect")
        Retry(sockfd);
    }
    //! 连接成功！
    else {
        YLOG_TRACE("In Connector::HandleWrite(), 连接服务器成功<{}:{}>", m_ServerAddr->GetIPStr().c_str(), m_ServerAddr->GetPort())
        SetState(eConnected);
        if(m_IsStarted) {
            m_NewConnectionCallback(sockfd);
        } else {
            SocketApiWrapper::close(sockfd);
        }
    }
}

void Connector::HandleError() {
    //!
    if(m_State != eConnecting)
        return;

    SocketApiWrapper::socket_t sockfd = RemoveAndResetChannel();
    int err = SocketApiWrapper::get_socket_error(sockfd);
    YLOG_ERROR("In Connector::HandleError(), Socket Error: {}", yy::util::GetErrorInfo(err))
    Retry(sockfd);
}

void Connector::Connecting(SocketApiWrapper::socket_t sockfd) {
    YLOG_TRACE("In Connector::Connecting, sockfd = {}", sockfd)
    SetState(eConnecting);
    m_Channel.reset(new IOChannel(m_Loop, sockfd, "Connector Channel"));
    m_Channel->SetWriteCallback([this](){ this->HandleWrite(); });
    m_Channel->SetErrorCallback([this](){ this->HandleError(); });
    m_Channel->EnableWriting();
}

void Connector::Retry(SocketApiWrapper::socket_t sockfd) {
    SocketApiWrapper::close(sockfd);
    SetState(eDisconnected);

    if(m_IsStarted)
    {
        YLOG_TRACE("In Connector::Retry, <sockfd:{}>服务器连接失败，将在 {} 秒后重连<{}:{}>",
                   sockfd, m_RetryDelay.count() / 1000.0, m_ServerAddr->GetIPStr().c_str(), m_ServerAddr->GetPort())

        m_NextRetryTimerID = m_Loop->RunAfter(m_RetryDelay, [this](){ this->StartInLoop(); });


        m_RetryDelay = (m_RetryDelay * 2 < kMaxRetryDelay) ? m_RetryDelay * 2 : kMaxRetryDelay;
    }
}

int Connector::RemoveAndResetChannel() {
    YLOG_TRACE("In Connector::RemoveAndResetChannel()")
    //! 取消监听连接Channel的事件，并释放之，但是sockfd仍处于开启的状态，因此需要返回sockfd并根据其具体情况关闭之或传递到上层
    m_Channel->ResetAndRemoveFromPoller();
    SocketApiWrapper::socket_t sockfd = m_Channel->GetFD();

    //! 在HandleWrite和HandleError中调用RemoveAndResetChannel时，
    //! m_Loop正在执行Channel->HandleHappenedEvent，若此时释放Channel，便会造成错误，因此要放入代办函数
    m_Loop->EnqueueCallbackInLoop([this](){ this->m_Channel.reset(); });
    return sockfd;
}

Connector::~Connector() {
    if(m_Channel) {
        m_Channel->ResetAndRemoveFromPoller();
    }

}


}
