#include "UdpTransport.h"
#include "Timestamp.h"
#include "IOChannel.h"
#include "Socket.h"
#include "EventLoop.h"
#include "log.h"
#include "status/Status.h"
#include "ErrnoSaver.h"
#include "IPAddress.h"
#include "LinearBuffer.h"
#include "ThreadPool.h"
#include "NetBuffer.h"

namespace yy::net {

using namespace yy::util;
using SocketApiWrapper::SocketError;

UdpTransport::UdpTransport(EventLoop * recvLoop, const IPAddressPtr & recv_addr, const int32_t recv_bytes_one, const int32_t m_send_thread_num):
    m_recv_bytes_one(recv_bytes_one),
    m_send_thread_num(m_send_thread_num),
    m_recvLoop(recvLoop),
    m_socket (std::make_unique<Socket>(Socket::Type::UDP, Socket::Family::IPv4, true)),
    m_sendWorkThreadPool(std::make_unique<ThreadPool>("UdpTransport Send")),
    m_recvBuf(std::make_unique<NetBuffer>(recv_bytes_one))
{
    assert(recvLoop);
    assert(m_socket);
    assert(m_recvBuf);
    assert(m_sendWorkThreadPool);

    m_sendWorkThreadPool->Start(m_recvLoop, m_send_thread_num);

    m_socket->SetOpt_ReuseAddr(true);
    m_socket->Bind(recv_addr);
    m_socket->SetOpt_KeepAlive(true);

    m_channel = std::make_unique<IOChannel>(recvLoop, m_socket->GetFD(), "udp-server-sock");

}



UdpTransport::~UdpTransport() {
    //! FIXED 注意： ~UdpSession() 并不确定执行的线程，因此不能在析构时执行可能带有AssertInLoopingThread()的函数
    m_sendWorkThreadPool->Stop();
    YLOG_DEBUG("UdpTransport<{}>已被析构！", this->GetSocketFD())
}

void UdpTransport::StartRecv()
{
    m_recvLoop->RunCallbackInLoop([this] { this->StartRecvInLoop(); });
}


void UdpTransport::StartRecvInLoop()
{
    m_recvLoop->AssertInLoopingThread();

    m_channel->SetErrorCallback([this](){this->HandleError();});
    m_channel->SetReadCallback ([this](){this->HandleRead() ;});
    m_channel->EnableReading();
    const auto recv_addr = m_socket->GetLocalAddr();
    YLOG_INFO("线程<{}>开始接收Udp，接收地址为：<{}:{}>，接收套接字为{}", util::CastThreadIDToStr(m_recvLoop->GetThreadID()),
          recv_addr->GetIPStr().c_str(), recv_addr->GetPort(), m_socket->GetFD())
}

SocketApiWrapper::socket_t UdpTransport::GetSocketFD() const {
    return m_socket->GetFD();
}

auto UdpTransport::GetRecvAddr() const -> IPAddressPtr
{
    return m_socket->GetLocalAddr();
}

void UdpTransport::SendUDP(const std::string_view & message, const IPAddressPtr & peerAddr) {
    //! 需要将数据从业务线程拷贝到IO线程中（否则线程不安全），这里SendInLoop使用const &延长临时对象生命周期
    m_sendWorkThreadPool->PushTask([this, buf = std::string(message), peerAddr]() {
        this->SendUDPWorker(std::string_view{buf}, peerAddr);
    });
}

void UdpTransport::SendUDP(const std::shared_ptr<LinearBuffer>& buf, const IPAddressPtr & peerAddr)
{
    m_sendWorkThreadPool->PushTask([this, buf /* 引用计数+1 */, peerAddr]() {
        this->SendUDPWorker(buf, peerAddr);
    });
}

void UdpTransport::SendUDPWorker(const std::string_view & buf, const IPAddressPtr & peerAddr) { //! const &延长临时对象生命周期
    //! 输出缓冲中目前没有任何的未发送数据，便直接向套接字发送数据（不借助输出缓冲）
    SocketApiWrapper::SocketResult rst = m_socket->Sendto(buf.data(), buf.size(), 0, peerAddr);

    if(rst.HasNoError()) {
        YLOG_TRACE("<{}>UdpSession::SendUDPWorker：直接将长{}B数据包发送给用户", m_socket->GetFD(), rst.Result())
    } else {
        YLOG_ERROR("<{}>UdpSession::SendUDPWorker, send error: {}", m_socket->GetFD(), rst.GetErrorInfo())

        switch (rst.ErrorCode()) {
            case SocketError::eAgain: {
                //! 不要将数据放入缓冲区稍后再发，因为此时数据不具有实时性且必定乱序（除非想自行实现可靠UDP）
            }
            case SocketError::eInterrupted:
                SendUDPWorker(buf, peerAddr);
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
                YLOG_ERROR("SendUDPWorker() 发生错误<{}>", rst.GetErrorInfo());
                HandleError();
                break;
        }
    }
}

void UdpTransport::SendUDPWorker(const std::shared_ptr<LinearBuffer> & buf, const IPAddressPtr & peerAddr) { //! const &延长临时对象生命周期
    //! 输出缓冲中目前没有任何的未发送数据，便直接向套接字发送数据（不借助输出缓冲）
    SocketApiWrapper::SocketResult rst = m_socket->Sendto(buf->Peek(), buf->GetDataSize(), 0, peerAddr);

    if(rst.HasNoError()) {
        YLOG_TRACE("<{}>UdpSession::SendUDPWorker：直接将长{}B数据包发送给用户", m_socket->GetFD(), rst.Result())
    } else {
        YLOG_ERROR("<{}>UdpSession::SendUDPWorker, send error: {}", m_socket->GetFD(), rst.GetErrorInfo())

        switch (rst.ErrorCode()) {
        case SocketError::eAgain: {
            //! 不要将数据放入缓冲区稍后再发，因为此时数据不具有实时性且必定乱序（除非想自行实现可靠UDP）
        }
        case SocketError::eInterrupted:
            SendUDPWorker(buf, peerAddr);
            break;
            //! UDP调用recvfrom或sendto不会返回该错误，但是如果对方
        case SocketError::eConnectionReset:
        case SocketError::eConnectionRefused:
            YLOG_ERROR("Send: ICMP Port Unreachable<{}:{}>", peerAddr->GetIPStr(), peerAddr->GetPortStr(), rst.GetErrorInfo());
            break;
        case SocketError::eMsgSize:
            YLOG_ERROR("UDP packet too large to send ({} bytes): {}", buf->GetDataSize(), rst.GetErrorInfo());
            break;
        default:
            // 其他错误，记录日志，关闭连接
            YLOG_ERROR("SendUDPWorker() 发生错误<{}>", rst.GetErrorInfo());
            HandleError();
            break;
        }
    }
}

void UdpTransport::HandleError() {
    m_recvLoop->AssertInLoopingThread();
}



void UdpTransport::HandleRead() {
    m_recvLoop->AssertInLoopingThread();

    YLOG_TRACE("正在读取来自连接<{}>的数据！", m_socket->GetFD())

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

    if(rst.HasNoError())
    {
        YLOG_TRACE("收到Udp数据[{}:{}]", peerAddr->GetIPStr(), peerAddr->GetPortStr());

        m_UdpRecievedCallback(*m_recvBuf, peerAddr);
    } else {
        //
    }
}




SocketApiWrapper::SocketResult UdpTransport::HandleRead_ET(IPAddressPtr & peerAddr) {
    m_recvLoop->AssertInLoopingThread();

    SocketApiWrapper::SocketResult rst;

    //! 边缘触发：只会到数据到来时触发一次可读事件，之后不管这些数据是否未被读取完，都不会再触发可读事件，因此一次事件需要一直读取套接字直到没有数据(EAGAIN)
    while(true)
    {
        //! ET读取
        // m_recvBuf->RecvFromSocket(m_socket, m_recv_bytes_one, rst, peerAddr);
        m_recvBuf->RecvAllFromSocket(m_socket, rst, peerAddr);

        YLOG_TRACE("<{}>UdpSession::HandleRead_ET(): 读取<{}>字节", m_socket->GetFD(), rst.Result())

        // 数据读取完毕
        if (rst.HasError())
        {
            const auto err = rst.ErrorCode();
            switch (err) {
                //! 接收正常结束： EAGAIN，表示已无数据可recv，即数据全部recv完毕
                case SocketError::eAgain:
                    rst = {};
                    break;
                //! 再次尝试：recv被信号打断，应重试
                case SocketError::eInterrupted:
                    continue;
                default:
                    YLOG_ERROR("<{}>UdpSession::HandleRead_:ET(): Recvfrom() error: {}", m_socket->GetFD(), rst.GetErrorInfo())
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
    m_recvBuf->RecvAllFromSocket(m_socket, rst, peerAddr);

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
