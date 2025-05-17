#include "UdpServer.h"
#include "EventLoopThreadPool.h"
#include "EventLoop.h"
#include "ConfigManager.h"
#include "log.h"
#include "IOChannel.h"
#include "Socket.h"
#include "UdpTransport.h"
#include "UdpSession.h"
#include "AppXmlConfig.h"


namespace yy::net {

#ifdef ____LINUX

class SignalManager {
public:
    SignalManager(EventLoop * loop, std::function<void()> handler)
        : loop_{loop}, channel_(std::make_unique<Channel>(loop_, SignalManager::pipe_.sideR(), "Wakeup Eventfd Channel"))
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
    std::unique_ptr<Channel> channel_;
};

FullDuplexPipe SignalManager::pipe_{};


#endif






UdpServer::UdpServer(EventLoop *mainLoop, bool reusePort) noexcept
    : m_mainLoop(mainLoop)
    , m_IOThreadPool( new EventLoopThreadPool(mainLoop) )
#ifdef ____LINUX
    , m_SignalManager{std::make_unique<SignalManager>(mainLoop, [this](){ this->HandleSignal(); })}
#endif
{
#ifdef ____LINUX
    util::set_signal_ignore(SIGPIPE);
#endif
}

UdpServer::~UdpServer() {
    m_mainLoop->AssertInLoopingThread(__FILE__, __LINE__);
    m_IsStarted = false;
}

void UdpServer::Start(int ioThreadNum, Milliseconds ioWaitTimeout, F_ThreadInitCallback cb) {
    if(!m_IsStarted.exchange(true)) {
        m_IOThreadPool->Start(ioThreadNum, ioWaitTimeout, cb);
    }

    m_udpTran = std::make_unique<UdpTransport>(m_IOThreadPool->GetNextLoop());
    assert(m_udpTran);
    m_udpTran->SetUdpRecievedCallback(std::bind_front(&UdpServer::HandleNewMessage, this));

}


void UdpServer::Stop() {
}

void UdpServer::HandleNewMessage(Buffer & recvBuf, IPAddressPtr peerAddr) {
    assert(m_udpTran);
    auto name = std::format("{}:{}", peerAddr->GetIPStr(), peerAddr->GetPortStr());
    UdpSessionPtr udpSession = std::make_shared<UdpSession>(name, *m_udpTran, peerAddr);

    //! ProtobufUdpCodec::OnData
    m_MessageCallback(udpSession, recvBuf);
}




void UdpServer::HandleSignal() {
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

