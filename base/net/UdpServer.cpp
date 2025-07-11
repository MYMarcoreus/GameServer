#include "UdpServer.h"
#include "EventLoopThreadPool.h"
#include "EventLoop.h"
#include "ConfigManager.h"
#include "log.h"
#include "IOChannel.h"
#include "Socket.h"
#include "UdpTransport.h"
#include "UdpSession.h"


#ifdef ____LINUX
#include "FullDuplexPipe.h"
#endif

namespace yy::net {






UdpServer::UdpServer(EventLoop *mainLoop, bool reusePort, const uint16_t app_udp_port, const int32_t recv_bytes_one, const int32_t m_send_thread_num, const uint8_t init_xor_code) noexcept:
    m_udp_port{app_udp_port},
    m_recv_bytes_one{recv_bytes_one},
    m_send_thread_num{m_send_thread_num},
    m_init_xor_code{init_xor_code},
    m_mainLoop(mainLoop),
    m_recvEventThreadPool(std::make_unique<EventLoopThreadPool>(mainLoop))
{

}

UdpServer::~UdpServer() {
    m_mainLoop->AssertInLoopingThread();
    m_IsStarted = false;
}

void UdpServer::Start(int ioThreadNum, const Milliseconds ioWaitTimeout, const F_ThreadInitCallback& cb) {
    if(!m_IsStarted.exchange(true)) {
        m_recvEventThreadPool->Start(1, ioWaitTimeout, cb);
        m_udpTran = std::make_unique<UdpTransport>(
            m_recvEventThreadPool->GetNextLoop(),
            m_udp_port,
            m_recv_bytes_one,
            m_send_thread_num
        );
        m_udpTran->SetUdpRecievedCallback([this](NetBuffer & recvBuf, const IPAddressPtr& peerAddr){ this->HandleNewMessage(recvBuf, peerAddr); });
    }
}


void UdpServer::Stop() {
}

void UdpServer::HandleNewMessage(NetBuffer & recvBuf, IPAddressPtr peerAddr) {
    assert(m_udpTran);
    auto name = std::format("{}:{}", peerAddr->GetIPStr(), peerAddr->GetPortStr());
    const UdpSessionPtr udpSession = std::make_shared<UdpSession>(0, *m_udpTran, peerAddr, m_init_xor_code);

    //! ProtobufUdpCodec::OnData
    m_MessageCallback(udpSession, recvBuf);
}








}

