#include "UdpSession.h"
#include "Buffer.h"
#include "UdpTransport.h"
#include "AppXmlConfig.h"

namespace yy::net {

UdpSession::UdpSession(std::string name, UdpTransport &udpTran, IPAddressPtr peerAddr)
        : m_name(name),
        m_xorCode{config::g_app_config->GetValue().app_xor_code()},
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