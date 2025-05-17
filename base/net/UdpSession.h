#ifndef GAMESERVER_UDPSESSION_H
#define GAMESERVER_UDPSESSION_H

#include "net_definations.h"

namespace google::protobuf {
class Message;
}

namespace yy::net {

class UdpSession {
public:
    UdpSession(std::string name, UdpTransport &udpTran, IPAddressPtr peerAddr);

    virtual ~UdpSession();

    ///Region 发送UDP数据：将待发送数据message添加至输出缓冲中（如果输出缓冲为空，则直接发送，无需等待事件触发）
    // 提供给codec的接口，包含自带首部的数据
    void SendUDP(const Buffer & buf);

    // 如果无需自定义首部，直接发送消息，可以调用以下接口
    void SendUDP(const void * buf, size_t len);
    void SendUDP(const std::shared_ptr<google::protobuf::Message> & message);
    void SendUDP(const google::protobuf::Message & message);

    // 以上所有接口会调用的函数，使用string_view表示只读内存区域
    void SendUDP(const std::string_view & message);
    ///End

    void SetXorCode(uint8_t xorCode) { m_xorCode = xorCode; }

    uint8_t                     GetXorCode()       const { return m_xorCode; }
    const std::string &         GetName()      const { return m_name; }


private:
    std::string             m_name;
    uint8_t                 m_xorCode;
    UdpTransport &          m_udpTran;
    IPAddressPtr            m_peerAddr;
};

}

#endif //GAMESERVER_UDPSESSION_H
