#ifndef GAMESERVER_UDPSESSION_H
#define GAMESERVER_UDPSESSION_H

#include "net_definations.h"

namespace google::protobuf {
class Message;
}

namespace yy::net {

class UdpSession final
{
public:
    UdpSession(uint64_t name, UdpTransport &udpTran, const IPAddressPtr& peerAddr, uint8_t xor_code);

    ~UdpSession();

    ///Region 发送UDP数据：将待发送数据message添加至输出缓冲中（如果输出缓冲为空，则直接发送，无需等待事件触发）
    void SendUDP(const void * buf, size_t len);
    void SendUDP(const std::string_view & message);
    ///End

    void SetXorCode(uint8_t xorCode) { m_xorCode = xorCode; }

    uint8_t          GetXorCode()   const { return m_xorCode; }
    uint64_t         GetName()      const { return m_tcpConnID; }


private:
    uint64_t                m_tcpConnID;
    uint8_t                 m_xorCode;
    UdpTransport &          m_udpTran;
    IPAddressPtr            m_peerAddr;
};

}

#endif //GAMESERVER_UDPSESSION_H
