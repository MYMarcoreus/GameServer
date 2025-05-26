#include "UdpSession.h"
#include "UdpTransport.h"
#include "Buffer.h"

namespace yy::net {

UdpSession::UdpSession(std::string name, UdpTransport &udpTran, const IPAddressPtr& peerAddr, const uint8_t xor_code)
        : m_name(std::move(name)),
        m_xorCode{xor_code},
        m_udpTran(udpTran),
        m_peerAddr(peerAddr)
{
    //
}

UdpSession::~UdpSession() {

}

void UdpSession::SendUDP(const void *buf, size_t len) {
    m_udpTran.SendUDP(buf, len, m_peerAddr);
}

void UdpSession::SendUDP(const std::string_view &message) {
    m_udpTran.SendUDP(message, m_peerAddr);
}




}