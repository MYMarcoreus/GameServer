#include "UdpSession.h"
#include "UdpTransport.h"

namespace yy::net {

UdpSession::UdpSession(uint64_t name, UdpTransport &udpTran, const IPAddressPtr& peerAddr, const uint8_t xor_code)
        : m_tcpConnID(name),
        m_xorCode{xor_code},
        m_udpTran(udpTran),
        m_peerAddr(peerAddr)
{
    //
}

UdpSession::~UdpSession() {

}

void UdpSession::SendUDP(const std::string_view &message)
{
    m_udpTran.SendUDP(message, m_peerAddr);
}

void UdpSession::SendUDP(const std::shared_ptr<util::LinearBuffer>& buf)
{
    m_udpTran.SendUDP(buf, m_peerAddr);
}
}
