#include "TcpConnection.h"
#include "Timestamp.h"
#include "Channel.h"
#include "Socket.h"
#include "EventLoop.h"
#include "log.h"
#include "status/Status.h"
#include "AppXmlConfig.h"

namespace yy::net {

using namespace yy::util;
using namespace yy::config;


TcpConnection::TcpConnection(std::string name, EventLoop *loop, SocketApiWrapper::socket_t sockfd,
                             IPAddress::ptr localAddr, IPAddress::ptr peerAddr)
    : m_Name(name),
      m_Loop(loop),
      m_Channel(std::make_unique<Channel>(loop, sockfd, name)),
      m_Socket (std::make_unique<Socket>(sockfd, Socket::Type::TCP, Socket::Family::IPv4)),
      m_LocalAddr(localAddr),
      m_PeerAddr(peerAddr),
      m_SendBuf(g_app_config->GetValue().send_bytes_max()),
      m_RecvBuf(g_app_config->GetValue().recv_bytes_max()),
      m_TempRecvBuf(g_app_config->GetValue().recv_bytes_one()),
      m_XorCode{g_app_config->GetValue().app_xor_code()}
{
    m_Channel->SetReadCallback ([this](){this->HandleRead() ;});
    m_Channel->SetWriteCallback([this](){this->HandleWrite();});
    m_Channel->SetCloseCallback([this](){this->HandleClose();});
    m_Channel->SetErrorCallback([this](){this->HandleError();});
    m_Socket->SetOpt_KeepAlive(true);

    SetState(eConnecting); //! 该结构在Accept接受连接成功后创建，此时TCP连接虽然已建立，但是回调函数未设置完毕，因此需要等待一下
    m_ConnectedTime.SetNow();
    m_ShudownTime.SetNow();
}



TcpConnection::~TcpConnection() {
    YLOG_DEBUG("连接<%d: %s>已被析构！", this->GetSocketFD(), m_Name.c_str())
}





void TcpConnection::Send(const void *buf, size_t len) {
    Send( std::string_view((char*)buf, len) ); //! const引用延长临时对象生命周期
}

void TcpConnection::Send(const Buffer &buf) {
    Send(buf.Peek(), buf.GetDataSize());
}

void TcpConnection::Send(const google::protobuf::Message * message) {
    Send(std::string_view(message->SerializeAsString())); //! const引用延长临时对象生命周期
}

void TcpConnection::Send(const google::protobuf::Message & message) {
    Send(std::string_view(message.SerializeAsString())); //! const引用延长临时对象生命周期
}

void TcpConnection::Send(const std::string_view & message) {
    if(m_Loop->IsInLoopingThread()) {
        m_Loop->RunCallbackInLoop([conn = shared_from_this(), &message](){ conn->SendInLoop(message); });
    } else {
        //! 需要将数据拷贝到IO线程中（否则线程不安全），这里SendInLoop使用const引用延长临时对象生命周期
        m_Loop->RunCallbackInLoop([conn = shared_from_this(), &message](){ conn->SendInLoop(std::string(message)); });
    }
}

void TcpConnection::SendInLoop(const std::string_view &buf) { //! const引用延长临时对象生命周期
    m_Loop->AssertInLoopingThread();

    ssize_t nByteSend = 0;
    ssize_t nByteRemained = buf.size();

    //! 输出缓冲中目前没有任何的未发送数据，则直接向套接字发送数据（不借助输出缓冲）
    if(!m_Channel->IsEnableWriting() && m_SendBuf.GetDataSize() == 0) {
        nByteSend = m_Socket->Send(buf.data(), buf.size());
        YLOG_TRACE("<%d>TcpConnection::SendInLoop：直接将长%zdB数据包发送给用户, head-tail==%zu-%zu",
                   m_Socket->GetFD(), nByteSend, m_SendBuf.GetHead(), m_SendBuf.GetTail())
        if(nByteSend >= 0) {
            nByteRemained = buf.size() - nByteSend;
            if(nByteRemained == 0 and m_ConnectionWriteCompleteCallback) {
                // 捕获shared_from_this()以延长生命周期
                m_Loop->EnqueueCallbackInLoop([this, self = shared_from_this()](){this->m_ConnectionWriteCompleteCallback(self);});
            }
        } else {
            nByteSend = 0;
            YLOG_ERROR("In TcpConnection::SendInLoop, send error: %s", util::StatusCode(errno).ToString().c_str())
        }
    }

    //! 将剩余的数据放入SendBuf中
    if(nByteRemained > 0) {
        bool isOk = m_SendBuf.AppendDataFromCBuffer(buf.data() + nByteSend, nByteRemained);

        if(isOk and !m_Channel->IsEnableWriting()) {
            m_Channel->EnableWriting();
        }
    }
}

void TcpConnection::Shutdown() {
    if(not CanShutdown())
        return;

    m_Loop->RunCallbackInLoop([self = shared_from_this()](){ self->ShutdownInLoop();});
}
void TcpConnection::ShutdownInLoop() {
    m_Loop->AssertInLoopingThread();

    YLOG_TRACE("TcpConnection::ShutdownInLoop(): CanShutdown()==%d", CanShutdown())

    if(not CanShutdown())
        return;

    YLOG_TRACE("TcpConnection::ShutdownInLoop(): Shutdown<%d>", GetSocketFD())

    m_ShudownTime.SetNow();
    m_Channel->DisableAllEvent();
    m_Channel->RemoveFromLoop();
    m_Socket->Shutdown();
    SetState(eShutdown);

    if(m_ConnectionShutdownCallback) {
        m_ConnectionShutdownCallback(shared_from_this());
    }
    //FIXME：不要加这一段，套接字需要统一在某个时刻关闭，且不能和Accept连接同时运行，否则会造成严重的bug！！！！！！！！！
    // m_Loop->RunAfter(Seconds{g_app_config->GetValue().close_delay()}, [this](){
    //     this->Close();
    //     YLOG_INFO("TcpConnection::ShutdownInLoop(): 时辰已到，正式关闭用户连接，回收套接字<%d>资源！", this->GetSocketFD())
    // });
}


void TcpConnection::Close() {
    if(not CanClose()) {
        YLOG_TRACE("In TcpConnection::CloseInLoop(): 套接字<%d>还不能关闭", this->GetSocketFD())
        return;
    }

    // m_Loop->EnqueueCallbackInLoop(std::bind(&TcpConnection::CloseInLoop, shared_from_this()));
    m_Loop->EnqueueCallbackInLoop([self = shared_from_this()](){self->CloseInLoop();});
}
void TcpConnection::CloseInLoop() {
    m_Loop->AssertInLoopingThread();

    if(not CanClose()) {
        return;
    }

    YLOG_TRACE("TcpConnection::CloseInLoop(): Fake Close<%d>", GetSocketFD())

    m_Channel->DisableAllEvent();
    m_Channel->RemoveFromLoop();
    SetState(eDisconnected);
    //m_Socket->Close(); //FIXME 不关闭，让析构函数调用Socket的析构函数来close；

    if(m_ConnectionCloseCallback) { // m_ConnectionCloseCallback == RemoveConnection
        m_ConnectionCloseCallback(shared_from_this());
    }
}

void TcpConnection::ConnectionEstablished() {
    m_Loop->AssertInLoopingThread();
    assert(IsConnecting());
    YLOG_TRACE("====================In TcpConnection::ConnectionEstablished：TCP连接完成！====================")

    //! 设置状态和Channel
    m_Channel->EnableReading();
    m_Channel->Tie(shared_from_this());
    SetState(eConnected);

    //! 执行上层回调
    if(m_ConnectionEstablishedCallback) {
        m_ConnectionEstablishedCallback(shared_from_this());
    }
}

void TcpConnection::ConnectionDestroyed() {
    m_Loop->AssertInLoopingThread();

    YLOG_DEBUG("In TcpConnection::ConnectionDestroyed(): ")

    //! 设置状态和Channel
    m_Channel->DisableAllEvent();
    m_Channel->RemoveFromLoop();
    SetState(eDisconnected);


    //! 执行上层回调
    // if(m_ConnectionDestroyedCallback) {
    //     m_ConnectionDestroyedCallback(shared_from_this());
    // }
}


void TcpConnection::HandleRead() {
    m_Loop->AssertInLoopingThread();

    YLOG_TRACE("正在读取来自连接<%d>的数据！", this->m_Socket->GetFD())

    //! ET读取数据到recvBuf中
    bool isReadOK = HandleRead_ET();

    if(isReadOK) {
        YLOG_TRACE("TcpConnection::HandleRead()<%d>: 数据接收完毕 head-tail==%zu-%zu",
                   m_Socket->GetFD(), m_RecvBuf.GetHead(), m_RecvBuf.GetTail());

        //FIXME m_MessageCallback的实际任务可能不在本线程运行（在线程池处理消息）！m_MessageCallback可能立即返回，因此需要拷贝数据！
        m_MessageCallback(shared_from_this(), m_RecvBuf);

        //m_RecvBuf.SetIsCompleted(true); // Receiver线程标记数据接收完成，Handler可处理

        // FIXME 如果m_MessageCallback的实际任务不在本线程运行，而是在其他线程（如线程池中的线程），那么该会导致其他线程中的buf已被Reset
        //m_RecvBuf.Reset();
    }
}

void TcpConnection::HandleWrite() {
    m_Loop->AssertInLoopingThread();

    if(!m_Channel->IsEnableWriting()) {
        YLOG_TRACE("未监听写事件，跳过")
        return;
    }

    if(IsShutdown() or !IsConnected()) {
        return;
    }

    auto nByteSend = m_SendBuf.RetrieveDataIntoSocket(m_Socket->GetFD());
    if (nByteSend > 0)
    {
        //! 数据全部发送完毕
        if (m_SendBuf.GetDataSize() == 0)
        {
            YLOG_TRACE("<%d>TcpConnection::HandleWrite：长%zdB数据包发送给用户, head-tail==%zu-%zu",
                       m_Socket->GetFD(), nByteSend, m_SendBuf.GetHead(), m_SendBuf.GetTail())

            //! 禁止监听写事件
            m_Channel->DisableWriting();

            //! 执行写回调
            if (m_ConnectionWriteCompleteCallback) {
                m_Loop->EnqueueCallbackInLoop([this, self = shared_from_this()](){this->m_ConnectionWriteCompleteCallback(self);});
            }
        }
    }
    else if(nByteSend == 0)
    {
        //todo
    }
    else
    {
        YLOG_ERROR("In TcpConnection::HandleWrite, send() error: 可能是用户连接已关闭, head-tail==%zu-%zu",
                   m_SendBuf.GetHead(), m_SendBuf.GetTail())

        ShutdownInLoop();
    }

    // m_SendBuf.Reset();
}

void TcpConnection::HandleClose() {
    m_Loop->AssertInLoopingThread();

    //! 用户要关闭连接时，并不直接Close，而是先Shundown，等到一定时间之后再Close
    switch (m_ConnectionState)
    {
        case eShutdown:
        case eConnected:
            ShutdownInLoop();
            break;
        default:
            break;
    }
}

void TcpConnection::HandleError() {
    m_Loop->AssertInLoopingThread();

}

bool TcpConnection::HandleRead_ET() {
    m_Loop->AssertInLoopingThread();

    bool isReadOk = false;

    while(true)
    {
        //! ET读取
        auto nBytesRecv = m_Socket->Recv(&*m_TempRecvBuf.begin(), m_TempRecvBuf.size());

        YLOG_TRACE("TcpConnection::HandleRead_ET(): 读取<%zd>字节，errno<%s>",
                   nBytesRecv, StatusCode{errno}.ToString().c_str())

        // 数据读取完毕
        if (nBytesRecv < 0) {
            YLOG_TRACE("TcpConnection::HandleRead_ET(): 数据读取完毕，errno<%s>", StatusCode{errno}.ToString().c_str())
            //! EAGAIN or EWOULDBLOCK: Recv Done
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                isReadOk = true;
                break;
            }
            //! ECONNRESET: Connection reset by peer
            else if(errno == ECONNRESET) {
                YLOG_INFO("TcpConnection::HandleRead_ET(): 用户连接Reset，关闭用户连接<%d>", m_Socket->GetFD())
                HandleClose();
                break;
            }
            else {
                YLOG_INFO("TcpConnection::HandleRead_ET()：发生错误<%s>", StatusCode{errno}.ToString().c_str())
                HandleError();
                break;
            }
        }
        // 连接关闭请求
        else if (nBytesRecv == 0) {
            YLOG_INFO("TcpConnection::HandleRead_ET(): 用户请求关闭，关闭用户连接<%d>", m_Socket->GetFD())
            HandleClose();
            break; //! FIXED_BUG 不要漏了，因为断开连接会回收userdata，Reset之，如果再循环一次会导致bad fd错误
        }
        // 不断读取数据
        else {
            // 将数据拷贝到用户数据缓冲区中
            bool isOk = m_RecvBuf.AppendDataFromCBuffer(&*m_TempRecvBuf.begin(), nBytesRecv);

            // 用户发送的数据大于能接收缓冲区的最大值，连接异常，关闭之
            if (!isOk) {
                YLOG_WARN("TcpConnection::HandleRead_ET(): 用户连接发送数据过多<%zu+%zu>%zu>，终止用户连接<%d>",
                          m_RecvBuf.GetDataSize(), (size_t) nBytesRecv, m_RecvBuf.GetMaxsize(), m_Socket->GetFD())
                HandleClose();
                break;
            }

            // 若本次接收的数据小于一次最多能接收的数据，说明本次接收是本批recv()的最后一份数据，可以直接结束本次recv()
            if (static_cast<size_t>(nBytesRecv) < m_TempRecvBuf.size()) {
                isReadOk = true;
                break;
            }
        }
    }
    return isReadOk;
}

int TcpConnection::GetSocketFD() const {
    return m_Socket->GetFD();
}



}