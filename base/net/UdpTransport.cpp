#include "UdpTransport.h"
#include "Timestamp.h"
#include "IOChannel.h"
#include "Socket.h"
#include "EventLoop.h"
#include "log.h"
#include "status/Status.h"
#include "AppXmlConfig.h"
#include "ErrnoSaver.h"
#include "IPAddress.h"
#include "ThreadPool.h"

#include <google/protobuf/message_lite.h>
#include <google/protobuf/message.h>

namespace yy::net {

using namespace yy::util;
using namespace yy::config;
using yy::SocketApiWrapper::SocketError;

UdpTransport::UdpTransport(EventLoop * eventLoop)
    : m_eventLoop(eventLoop),
      m_socket (std::make_unique<Socket>(Socket::Type::UDP, Socket::Family::IPv4, true)),
      m_channel(std::make_unique<IOChannel>(eventLoop, m_socket->GetFD(), "udp-server-sock")),
      m_recvBuf(g_app_config->GetValue().recv_bytes_one()),
      m_sendThreadPool("UdpTransport Send-ThreadPool")
{

    yy::net::IPAddressPtr recvAddr = std::make_shared<net::IPv4Address>(
            config::g_app_config->GetValue().app_udp_port());
    m_socket->Bind(recvAddr);
    m_socket->SetOpt_KeepAlive(true);
    m_channel->SetReadCallback ([this](){this->HandleRead() ;});
    m_channel->SetErrorCallback([this](){this->HandleError();});


    m_sendThreadPool.Start(m_eventLoop, g_app_config->GetValue().udp_io_thread_num());
}



UdpTransport::~UdpTransport() {
    //! FIXED 注意： ~UdpSession() 并不确定执行的线程，因此不能在析构时执行可能带有AssertInLoopingThread()的函数

    YLOG_DEBUG("UdpTransport<{}>已被析构！", this->GetSocketFD())
}


SocketApiWrapper::socket_t UdpTransport::GetSocketFD() const {
    return m_socket->GetFD();
}



void UdpTransport::SendUDP(const void *buf, size_t len, IPAddressPtr peerAddr) {
    SendUDP(std::string_view((char *) buf, len), peerAddr); // 使用 string_view 观察调用者提供的内存，生命周期由调用者保证
}

void UdpTransport::SendUDP(const Buffer &buf, IPAddressPtr peerAddr) {
    SendUDP(std::string_view(buf.Peek(), buf.GetDataSize()), peerAddr);
}

void UdpTransport::SendUDP(const google::protobuf::Message & message, IPAddressPtr peerAddr) {
    //! FIXED_BUG：string_view对象并不会延长临时string的生命周期
    //! 下面两种方式生成的临时string均会在message.SerializeAsString()返回后被销毁，string_view 持有的指针变成悬垂指针
    //! SendUDP(std::string_view(message.SerializeAsString()));
    //! SendUDP(message.SerializeAsString());

    std::string tmp = message.SerializeAsString(); // tmp 是一个局部变量，生命周期在本函数结束前有效（如果跨线程，则不安全需要拷贝该字符串）
    SendUDP(std::string_view{tmp}, peerAddr); // string 会自动转换构造为 string_view临时对象，被调用的Send不论有没有const&都是生命周期安全的。
}

void UdpTransport::SendUDP(const std::shared_ptr<google::protobuf::Message> &message, IPAddressPtr peerAddr) {
    if(message) { SendUDP(*message, peerAddr);  }
}

void UdpTransport::SendUDP(const std::string_view & message, IPAddressPtr peerAddr) {
    if(m_eventLoop->IsInLoopingThread()) {
        m_eventLoop->RunCallbackInLoop([message, peerAddr, this](){ this->SendUDPInLoop(message, peerAddr); });
    } else {
        //! 需要将数据从业务线程拷贝到IO线程中（否则线程不安全），这里SendInLoop使用const &延长临时对象生命周期
        m_sendThreadPool.PushTask([this, msg = std::string(message), peerAddr]() {
            this->SendUDPInLoop(std::string_view{msg}, peerAddr);
        });
    }
}

void UdpTransport::SendUDPInLoop(const std::string_view & buf, IPAddressPtr peerAddr) { //! const &延长临时对象生命周期
    //! 输出缓冲中目前没有任何的未发送数据，便直接向套接字发送数据（不借助输出缓冲）
    SocketApiWrapper::SocketResult rst = m_socket->Sendto(buf.data(), buf.size(), 0, peerAddr);

    if(rst.HasNoError()) {
        YLOG_TRACE("<{}>UdpSession::SendUDPInLoop：直接将长{}B数据包发送给用户", m_socket->GetFD(), rst.Result())
    } else {
        YLOG_ERROR("<{}>UdpSession::SendUDPInLoop, send error: {}", m_socket->GetFD(), rst.GetErrorInfo())

        switch (rst.ErrorCode()) {
            case SocketError::eAgain: {
                //! 不要将数据放入缓冲区稍后再发，因为此时数据不具有实时性且必定乱序（除非想自行实现可靠UDP）
            }
            case SocketError::eInterrupted:
                SendUDPInLoop(buf, peerAddr);
                break;
            //! UDP调用recvfrom或sendto不会返回该错误，但是如果对方
            case SocketError::eConnectionReset:
            case SocketError::eConnectionRefused:
                YLOG_ERROR("Send: ICMP Port Unreachable<{}:{}>", peerAddr->GetIPStr(), peerAddr->GetPortStr(), rst.GetErrorInfo());
                break;
            case SocketError::eMsgSize:
                YLOG_ERROR("UDP packet too large to send ({} bytes): {}", buf.size(), rst.GetErrorInfo());
                break;
            default:
                // 其他错误，记录日志，关闭连接
                YLOG_ERROR("SendUDPInLoop() 发生错误<{}>", rst.GetErrorInfo());
                HandleError();
                break;
        }
    }
}

void UdpTransport::HandleError() {
    m_eventLoop->AssertInLoopingThread(__FILE__, __LINE__);
}



void UdpTransport::HandleRead() {
    m_eventLoop->AssertInLoopingThread(__FILE__, __LINE__);

    YLOG_TRACE("正在读取来自连接<{}>的数据！", m_socket->GetFD())

    size_t out_nBytesRead = 0;
    IPAddressPtr peerAddr = std::make_shared<IPv4Address>();

    // 返回值为是否有逻辑错误（而非socket错误）：用户是否发送过多数据
#ifdef ____WINDOWS
    //! ET读取数据到recvBuf中
    auto rst = HandleRead_ET(peerAddr);
#elif defined(____LINUX)
    auto rst = HandleRead_ET(peerAddr);
#else
    #error Platform not supported
#endif

    if(rst.HasNoError()) {
        YLOG_TRACE("<{}>UdpSession::HandleRead(): 数据接收完毕 head-tail=={}-{}",
                   m_socket->GetFD(), m_recvBuf.GetHead(), m_recvBuf.GetTail());

        m_UdpRecievedCallback(m_recvBuf, peerAddr);
    } else {
        //
    }
}




SocketApiWrapper::SocketResult UdpTransport::HandleRead_ET(IPAddressPtr & peerAddr) {
    m_eventLoop->AssertInLoopingThread(__FILE__, __LINE__);

    SocketApiWrapper::SocketResult rst;

    //! 边缘触发：只会到数据到来时触发一次可读事件，之后不管这些数据是否未被读取完，都不会再触发可读事件，因此一次事件需要一直读取套接字直到没有数据(EAGAIN)
    while(true)
    {
        //! ET读取
        m_recvBuf.RecvFromSocket(m_socket, g_app_config->GetValue().recv_bytes_one(), rst, peerAddr);

        YLOG_TRACE("<{}>UdpSession::HandleRead_ET(): 读取<{}>字节", m_socket->GetFD(), rst.Result())

        // 数据读取完毕
        if (rst.HasError())
        {
            const auto err = rst.ErrorCode();
            YLOG_ERROR("<{}>UdpSession::HandleRead_:ET(): recv() error: {}", m_socket->GetFD(), rst.GetErrorInfo())
            switch (err) {
                //! 接收正常结束： EAGAIN，表示已无数据可recv，即数据全部recv完毕
                case SocketError::eAgain:
                    rst = {rst.Result(), 0};
                    break;
                //! 再次尝试：recv被信号打断，应重试
                case SocketError::eInterrupted:
                    continue;
                default:
                    YLOG_INFO("发生错误<{}>", rst.GetErrorInfo());
                    HandleError();
                    break;
            }

            //! 结束while循环
            break;
        }
    }
    return rst;
}

SocketApiWrapper::SocketResult UdpTransport::HandleRead_LT(IPAddressPtr & peerAddr) {
    SocketApiWrapper::SocketResult rst;
    m_recvBuf.RecvAllFromSocket(m_socket, rst, peerAddr);

    if (rst.HasError()) {
        YLOG_ERROR("<{}>UdpSession::HandleRead_:LT(): recv() error: {}", m_socket->GetFD(), rst.GetErrorInfo())
        const auto err = rst.ErrorCode();
        switch (err) {
            //! 接收正常结束： EAGAIN，表示已无数据可recv，即数据全部recv完毕
            case SocketError::eAgain:
                rst = {rst.Result(), 0};
                break;
            //! 再次尝试：recv被信号打断，应等待下一次的读事件
            case SocketError::eInterrupted:
                rst = {rst.Result(), 0};
                break;
            default:
                YLOG_INFO("发生错误<{}>", rst.GetErrorInfo());
                HandleError();
                break;
        }
    }

    return rst;
}






}
