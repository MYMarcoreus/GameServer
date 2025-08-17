#include "UdpServer.h"
#include "EventLoopThreadPool.h"
#include "EventLoop.h"
#include "ConfigManager.h"
#include "EventLoopThread.h"
#include "log.h"
#include "IOChannel.h"
#include "Socket.h"
#include "UdpTransporter.h"
#include "UdpSession.h"


#ifdef ____LINUX
#include "FullDuplexPipe.h"
#endif

namespace yy::net {






UdpServer::UdpServer(EventLoop *mainLoop, const IPAddressPtr& udp_addr, bool reusePort, const int32_t recv_bytes_one, const int32_t send_thread_num, const uint8_t init_xor_code) noexcept:
    m_recv_bytes_one{recv_bytes_one},
    m_send_thread_num{send_thread_num},
    m_init_xor_code{init_xor_code},
    m_mainLoop(mainLoop),
    m_recvLoopThread(std::make_unique<EventLoopThread>(nullptr, 500ms))
{
    m_udpTran = std::make_unique<UdpTransporter>(m_recvLoopThread->CreateLoop(), udp_addr, m_recv_bytes_one, m_send_thread_num);
}

UdpServer::~UdpServer() {
    m_mainLoop->AssertInLoopingThread();
    m_IsStarted = false;
}

void UdpServer::Start(int ioThreadNum, const Milliseconds ioWaitTimeout) {
    if(!m_IsStarted.exchange(true)) {
        m_udpTran->StartRecv();
        m_udpTran->SetUdpRecievedCallback([this](NetBuffer & recvBuf, const IPAddressPtr& peerAddr) {
            this->HandleNewMessage(recvBuf, peerAddr);
        });
    }
}


void UdpServer::Stop() {
}

IPAddressPtr UdpServer::GetRecvAddr() const
{
    return m_udpTran->GetRecvAddr();
}

UdpSessionPtr UdpServer::RegisterSession(uint64_t connid, IPAddressPtr udpAddr)
{
    const UdpSessionPtr session = std::make_unique<UdpSession>(connid, GetUdpTran(), udpAddr, m_init_xor_code);
    const auto host = session->GetPeerAddr()->ToString();

    util::WriteLockGuard lg{m_mutex};
    m_connid_to_host[session->GetConnID()] = host;
    m_host_to_session[host] = session;

    return session;
}

void UdpServer::UnregisterSession(const uint64_t connid)
{
    const auto host = m_connid_to_host[connid];
    util::WriteLockGuard lg{m_mutex};
    m_host_to_session.erase(host);
    m_connid_to_host.erase(connid);
}

void UdpServer::HandleNewMessage(NetBuffer & recvBuf, const IPAddressPtr& peerAddr) {
    assert(m_udpTran);

    auto host = peerAddr->ToString();
    const UdpSessionPtr udpSession = m_host_to_session[host];
    if (udpSession == nullptr) {
        YLOG_ERROR("UdpServer::HandleNewMessage: 未注册的Udp地址 ", host)
        return;
    }

    //! ProtobufUdpCodec::OnData
    m_MessageCallback(udpSession, recvBuf);
}








}

