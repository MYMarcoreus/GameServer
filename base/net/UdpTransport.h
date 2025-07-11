#ifndef LINUXGAMESERVER_UDPSESSION_H
#define LINUXGAMESERVER_UDPSESSION_H

#include "net_definations.h"
#include "Timestamp.h"
#include "socket_definations.h"

#include <memory>


namespace yy::util
{
class SequentialBuffer;
}

namespace google::protobuf {
class Message;
}




namespace yy::net {

class EventLoop;


class UdpTransport  {
    using F_UdpRecievedCallback = std::function<void(NetBuffer &, IPAddressPtr)>;
public:
    explicit UdpTransport(EventLoop * recvLoop, std::optional<uint16_t> app_udp_port, const int32_t recv_bytes_one, const int32_t m_send_thread_num);

    ~UdpTransport();

    ///Region 发送UDP数据：将待发送数据message添加至输出缓冲中（如果输出缓冲为空，则直接发送，无需等待事件触发）
    void SendUDP(const std::string_view & message, const IPAddressPtr & peerAddr);
    void SendUDP(const std::shared_ptr<util::SequentialBuffer> & buf, const IPAddressPtr & peerAddr);

    ///End

    ///Region GETTER
    EventLoop *                 GetLoop()          const { return m_recvLoop; }
    SocketApiWrapper::socket_t  GetSocketFD()      const ;
    ///End

    ///Region SETTER
    void SetUdpRecievedCallback(F_UdpRecievedCallback cb) { m_UdpRecievedCallback = std::move(cb); }
    ///End

private:
    uint16_t m_udp_port;
    int32_t  m_recv_bytes_one;
    int32_t  m_send_thread_num;

    void HandleRead();     // 将套接字的数据接收到RecvBuf中
    SocketApiWrapper::SocketResult HandleRead_ET(IPAddressPtr & peerAddr);  // 将套接字的数据接收到RecvBuf中
    SocketApiWrapper::SocketResult HandleRead_LT(IPAddressPtr & peerAddr);  // 将套接字的数据接收到RecvBuf中
    void HandleError();    // 处理错误

    void SendUDPWorker(const std::string_view &buf, const IPAddressPtr & peerAddr);
    void SendUDPWorker(const std::shared_ptr<util::SequentialBuffer> & buf, const IPAddressPtr & peerAddr);

private:
    EventLoop *                                     m_recvLoop;
    std::unique_ptr<class Socket>                   m_socket;
    std::unique_ptr<class IOChannel>                m_channel;
    std::unique_ptr<class ThreadPool>               m_sendWorkThreadPool;
    F_UdpRecievedCallback                           m_UdpRecievedCallback;                 //! 需要及时响应，立即执行

    std::unique_ptr<NetBuffer> m_recvBuf;
};

}
#endif //LINUXGAMESERVER_UDPSESSION_H

