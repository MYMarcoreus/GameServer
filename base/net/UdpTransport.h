#ifndef LINUXGAMESERVER_UDPSESSION_H
#define LINUXGAMESERVER_UDPSESSION_H

#include "net_definations.h"
#include "Buffer.h"
#include "Timestamp.h"
#include "ThreadPool.h"

#include <memory>


namespace google::protobuf {
class Message;
}




namespace yy::net {

class IOChannel;
class EventLoop;
class Socket;


class UdpTransport  {
    using F_UdpRecievedCallback = std::function<void(Buffer &, IPAddressPtr)>;
public:

    UdpTransport(EventLoop *eventLoop);

    ~UdpTransport();

    ///Region 发送UDP数据：将待发送数据message添加至输出缓冲中（如果输出缓冲为空，则直接发送，无需等待事件触发）
    // 提供给codec的接口，包含自带首部的数据
    void SendUDP(const Buffer & buf, IPAddressPtr peerAddr);

    // 如果无需自定义首部，直接发送消息，可以调用以下接口
    void SendUDP(const void * buf, size_t len, IPAddressPtr peerAddr);
    void SendUDP(const std::shared_ptr<google::protobuf::Message> & message, IPAddressPtr peerAddr);
    void SendUDP(const google::protobuf::Message & message, IPAddressPtr peerAddr);

    // 以上所有接口会调用的函数，使用string_view表示只读内存区域
    void SendUDP(const std::string_view & message, IPAddressPtr peerAddr);
    ///End

    ///Region GETTER
    EventLoop *                 GetLoop()          const { return m_eventLoop; }
    SocketApiWrapper::socket_t  GetSocketFD()      const ;
    ///End

    ///Region SETTER
    void SetUdpRecievedCallback(F_UdpRecievedCallback cb)                 { m_UdpRecievedCallback = cb; }
    ///End

private:

    void HandleRead();     // 将套接字的数据接收到RecvBuf中
    SocketApiWrapper::SocketResult HandleRead_ET(IPAddressPtr & peerAddr);  // 将套接字的数据接收到RecvBuf中
    SocketApiWrapper::SocketResult HandleRead_LT(IPAddressPtr & peerAddr);  // 将套接字的数据接收到RecvBuf中
    void HandleError();    // 处理错误

    void SendUDPInLoop(const std::string_view &buf, IPAddressPtr peerAddr);

private:
    EventLoop *                     m_eventLoop;
    std::unique_ptr<Socket>         m_socket;
    std::unique_ptr<IOChannel>      m_channel;
    yy::net::ThreadPool             m_sendThreadPool;
    F_UdpRecievedCallback           m_UdpRecievedCallback;                 //! 需要及时响应，立即执行

    Buffer m_recvBuf;
};

}
#endif //LINUXGAMESERVER_UDPSESSION_H

