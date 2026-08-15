#pragma once
#include "net_definations.h"

namespace yy::util
{
class LinearBuffer;
}

namespace google::protobuf {
class Message;
}

namespace yy::net {

class UdpSession final: public std::enable_shared_from_this<UdpSession>
{
public:
    UdpSession(uint64_t conn_id, UdpTransporter &udpTran, const IPAddressPtr& peerAddr, uint8_t xor_code);

    ~UdpSession();

    ///Region 发送UDP数据：将待发送数据message添加至输出缓冲中（如果输出缓冲为空，则直接发送，无需等待事件触发）
    void SendUDP(const std::string_view & message);
    void SendUDP(const std::shared_ptr<util::LinearBuffer> & buf);
    ///End

    void SetXorCode(const uint8_t xorCode) { m_xorCode = xorCode; }

    uint8_t          GetXorCode()   const { return m_xorCode; }
    uint64_t         GetConnID()      const { return m_tcpConnID; }
    IPAddressPtr GetPeerAddr() const { return m_peerAddr; }


private:
    uint64_t                m_tcpConnID;
    uint8_t                 m_xorCode;
    UdpTransporter &        m_udpTran;
    IPAddressPtr            m_peerAddr;
};

}
