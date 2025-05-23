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


#ifdef ____LINUX
#include "FullDuplexPipe.h"
#endif

namespace yy::net {






UdpServer::UdpServer(EventLoop *mainLoop, bool reusePort) noexcept
    : m_mainLoop(mainLoop)
    , m_recvEventThreadPool(std::make_unique<EventLoopThreadPool>(mainLoop))
{

}

UdpServer::~UdpServer() {
    m_mainLoop->AssertInLoopingThread(__FILE__, __LINE__);
    m_IsStarted = false;
}

void UdpServer::Start(int ioThreadNum, Milliseconds ioWaitTimeout, F_ThreadInitCallback cb) {
    if(!m_IsStarted.exchange(true)) {
        m_recvEventThreadPool->Start(1, ioWaitTimeout, cb);
        m_udpTran = std::make_unique<UdpTransport>(m_recvEventThreadPool->GetNextLoop());
        m_udpTran->SetUdpRecievedCallback(std::bind_front(&UdpServer::HandleNewMessage, this));
    }
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








}

