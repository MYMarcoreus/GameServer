#include "TcpConnection.h"
#include "Timestamp.h"
#include "Channel.h"
#include "Socket.h"
#include "EventLoop.h"
#include "log.h"
#include "status/Status.h"
#include "AppXmlConfig.h"
#include "ErrnoSaver.h"

#include <google/protobuf/message_lite.h>
#include <google/protobuf/message.h>

namespace yy::net {

using namespace yy::util;
using namespace yy::config;
using yy::SocketApiWrapper::SocketError;


TcpConnection::TcpConnection(std::string name, EventLoop *loop, SocketApiWrapper::socket_t sockfd,
                             IPAddress::ptr localAddr, IPAddress::ptr peerAddr)
    : m_Name(name),
      m_ioLoop(loop),
      m_Channel(std::make_unique<Channel>(loop, sockfd, name)),
      m_Socket (std::make_unique<Socket>(sockfd, Socket::Type::TCP, Socket::Family::IPv4)),
      m_LocalAddr(localAddr),
      m_PeerAddr(peerAddr),
      m_SendBuf(g_app_config->GetValue().send_bytes_one()),
      m_RecvBuf(g_app_config->GetValue().recv_bytes_one()),
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
    m_HeartTime.SetNow();
}



TcpConnection::~TcpConnection() {
    //! FIXED 注意： ~TcpConnection() 并不确定执行的线程，因此不能在析构时执行可能带有AssertInLoopingThread()的函数
    // m_Channel->DisableAllEvent();
    // m_Channel->RemoveFromLoop();

    YLOG_DEBUG("连接<{}: {}>已被析构！", this->GetSocketFD(), m_Name.c_str())
}


int TcpConnection::GetSocketFD() const {
    return m_Socket->GetFD();
}


void TcpConnection::Send(const void *buf, size_t len) {
    Send( std::string_view((char*)buf, len) ); //! const引用延长临时对象生命周期
}

void TcpConnection::Send(const Buffer &buf) {
    Send(buf.Peek(), buf.GetDataSize());
}

void TcpConnection::Send(const google::protobuf::Message & message) {
    Send(std::string_view(message.SerializeAsString())); //! const引用延长临时对象生命周期
}

void TcpConnection::Send(const std::shared_ptr<google::protobuf::Message> &message) {
    if(message) { Send(*message);  }
}

void TcpConnection::Send(const std::string_view & message) {
    if (not CanIO()) {
        YLOG_TRACE("<{}>In TcpConnection::Send, Connection Already Closed", m_Socket->GetFD())
        return;
    }

    if(m_ioLoop->IsInLoopingThread()) {
        m_ioLoop->RunCallbackInLoop([conn = shared_from_this(), message](){ conn->SendInLoop(message); });
    } else {
        //! 需要将数据拷贝到IO线程中（否则线程不安全），这里SendInLoop使用const引用延长临时对象生命周期
        m_ioLoop->RunCallbackInLoop([conn = shared_from_this(), msg = std::string(message)](){ conn->SendInLoop(msg); });
    }
}

void TcpConnection::SendInLoop(const std::string_view & buf) { //! const引用延长临时对象生命周期
    m_ioLoop->AssertInLoopingThread(__FILE__, __LINE__);

    if (not CanIO()) {
        YLOG_TRACE("<{}>In TcpConnection::SendInLoop, Connection Already Closed", m_Socket->GetFD())
        return;
    }

    //! 发送缓冲区仍有数据（实际上也伴随着注册了写事件），不执行SendInLoop()，而是等待写事件发生执行HandleWrite()
    if(m_Channel->IsEnableWriting() or m_SendBuf.GetDataSize() != 0)
        return;

    //! 输出缓冲中目前没有任何的未发送数据，便直接向套接字发送数据（不借助输出缓冲）
    SocketApiWrapper::SocketResult rst = m_Socket->Send(buf.data(), buf.size());

    if(rst.HasNoError()) {
        YLOG_TRACE("<{}>TcpConnection::SendInLoop：直接将长{}B数据包发送给用户, head-tail=={}-{}",
                m_Socket->GetFD(), rst.Result(), m_SendBuf.GetHead(), m_SendBuf.GetTail())

        auto nByteRemained = buf.size() - rst.Result();
        assert(nByteRemained >= 0);
        if(nByteRemained == 0) {
            //! 本次Send发送完了所有数据，执行写完毕回调
            // 捕获shared_from_this()以延长生命周期
            if(m_ConnectionWriteCompleteCallback)
                m_ioLoop->EnqueueCallbackInLoop([this, self = shared_from_this()](){this->m_ConnectionWriteCompleteCallback(self);});
        }
        else {
            //! Send无法一次发送完，仍有剩余的数据未发送，故将剩余的数据放入SendBuf中，注册写事件，让其在写事件发生时执行HandleWrite()
            bool isOk = m_SendBuf.AppendDataFromCBuffer(buf.data() + rst.Result(), nByteRemained);

            if(isOk) {
                if(not m_Channel->IsEnableWriting())
                    m_Channel->EnableWriting();
            } else {
                //todo 需要Send的数据太多，Send一次之后剩余的数据竟然不能放入SendBuf，是否应该关闭连接？
            }
        }

    } else {
        YLOG_ERROR("<{}>TcpConnection::SendInLoop, send error: {}", m_Socket->GetFD(), rst.GetErrorInfo())

        switch (rst.ErrorCode()) {
            case SocketError::eAgain:
            case SocketError::eInterrupted: {
                // 将数据放入缓冲区，注册可写事件，等到套接字可写后重新发送
                bool isOk = m_SendBuf.AppendDataFromCBuffer(buf.data(), buf.size());
                if (isOk) {
                    if (not m_Channel->IsEnableWriting())
                        m_Channel->EnableWriting();
                } else {
                    //todo 需要Send的数据太多，Send一次之后剩余的数据竟然不能放入SendBuf，是否应该关闭连接？
                }

                break;
            }
            case SocketError::eConnectionReset:
            case SocketError::eNotConnected:
            case SocketError::eConnectionAborted:
                // 连接异常，关闭连接
                HandleClose();
                break;
            default:
                // 其他错误，记录日志，关闭连接
                HandleError();
                break;
        }
    }
}

void TcpConnection::HandleWrite() {
    m_ioLoop->AssertInLoopingThread(__FILE__, __LINE__);

    if(!m_Channel->IsEnableWriting()) {
        YLOG_TRACE("未监听写事件，跳过")
        return;
    }

    if(not CanIO()) {
        return;
    }

    //! 该函数专门将写缓冲的数据写入socket
    auto rst = m_SendBuf.PopDataToSocket(m_Socket->GetFD());

    if(rst.HasNoError())
    {
        auto nByteSend = rst.Result();
        if (nByteSend > 0) // 缓冲区有可发送的数据
        {
            //! 数据全部发送完毕
            if (m_SendBuf.GetDataSize() == 0) {
                YLOG_TRACE("<{}>TcpConnection::HandleWrite：长{}B数据包发送给用户, head-tail=={}-{}",
                           m_Socket->GetFD(), nByteSend, m_SendBuf.GetHead(), m_SendBuf.GetTail())

                //! 禁止监听写事件
                m_Channel->DisableWriting();
            }
        }

        //! 执行写回调
        if (m_ConnectionWriteCompleteCallback) {
            m_ioLoop->EnqueueCallbackInLoop([this, self = shared_from_this()](){this->m_ConnectionWriteCompleteCallback(self);});
        }
    }
    else
    {
        YLOG_ERROR("<{}>TcpConnection::HandleWrite, send() error: {}", m_Socket->GetFD(), rst.GetErrorInfo())

        ShutdownInLoop();
    }

    // m_SendBuf.Reset();
}

void TcpConnection::Shutdown() {
    if(not CanShutdown())
        return;

    m_ioLoop->RunCallbackInLoop([self = shared_from_this()](){ self->ShutdownInLoop();});
}
void TcpConnection::ShutdownInLoop() {
    m_ioLoop->AssertInLoopingThread(__FILE__, __LINE__);

    YLOG_TRACE("<{}>TcpConnection::ShutdownInLoop(): CanShutdown()=={}", m_Socket->GetFD(), CanShutdown())

    if(not CanShutdown())
        return;

    YLOG_DEBUG("<{}>TcpConnection::ShutdownInLoop(): Shutdown", GetSocketFD())

    m_ShudownTime.SetNow();
    m_Channel->ResetAndRemoveFromPoller();
    // m_Socket->Shutdown();
    SetState(eShutdown);

    if(m_ConnectionShutdownCallback) {
        m_ConnectionShutdownCallback(shared_from_this());
    }

    SetState(eDisconnected);
    //m_Socket->Close(); //FIXME 不关闭，让析构函数调用Socket的析构函数来close；

    // TcpConnection::m_ConnectionCloseCallback(TcpConnection::ioLoop) == TcpServer::RemoveConnection --> m_AcceptorLoop->RunCallbackInLoop
    if(m_ConnectionCloseCallback) {
        m_ConnectionCloseCallback(shared_from_this());
    }

    //FIXME：不要加这一段，套接字需要统一在某个时刻关闭，且不能和Accept连接同时运行，否则会造成严重的bug！！！！！！！！！
    // m_Loop->RunTaskAfter(Seconds{g_app_config->GetValue().close_delay()}, [this](){
    //     this->Close();
    //     YLOG_INFO("TcpConnection::ShutdownInLoop(): 时辰已到，正式关闭用户连接，回收套接字<%d>资源！", this->GetSocketFD())
    // });
}


void TcpConnection::ConnectionEstablished() {
    m_ioLoop->AssertInLoopingThread(__FILE__, __LINE__);
    assert(IsConnecting());

    //! 设置状态和Channel
    m_Channel->EnableReading();
    m_Channel->Tie(shared_from_this()); //! 连接建立后，确保该连接的智能指针至少有一个引用
    SetState(eConnected);

    //! 执行的是TcpServer的回调，但是TcpServer的回调被上层GameServers设置为GameServer::OnConnectionEstablished
    if(m_ConnectionEstablishedCallback) {
        m_ConnectionEstablishedCallback(shared_from_this());
    }
}

void TcpConnection::ConnectionDestroyed() {
    m_ioLoop->AssertInLoopingThread(__FILE__, __LINE__);

    YLOG_DEBUG("In TcpConnection::ConnectionDestroyed(), {}, {}", ::yy::util::CastThreadIDToStr(m_ioLoop->GetThreadID()), ::yy::util::GetStrThreadID())

    //! 设置状态和Channel
    m_Channel->ResetAndRemoveFromPoller();
    SetState(eDisconnected);


    //! 执行上层回调
    // if(m_ConnectionDestroyedCallback) {
    //     m_ConnectionDestroyedCallback(shared_from_this());
    // }
}


void TcpConnection::HandleRead() {
    m_ioLoop->AssertInLoopingThread(__FILE__, __LINE__);

    if(not CanIO()) {
        return;
    }

    YLOG_TRACE("正在读取来自连接<{}>的数据！", m_Socket->GetFD())

    size_t out_nBytesRead = 0;

    // 返回值为是否有逻辑错误（而非socket错误）：用户是否发送过多数据
#ifdef ____WINDOWS
    //! ET读取数据到recvBuf中
    auto rst = HandleRead_ET();
#elif defined(____LINUX)
    auto rst = HandleRead_ET();
#else
    #error Platform not supported
#endif

    if(rst.HasNoError()) {
        YLOG_TRACE("<{}>TcpConnection::HandleRead(): 数据接收完毕 head-tail=={}-{}",
                   m_Socket->GetFD(), m_RecvBuf.GetHead(), m_RecvBuf.GetTail());

        m_HeartTime.SetNow();

        // FIXME m_MessageCallback的实际任务可能不在本线程运行（在线程池处理消息）！m_MessageCallback可能立即返回，因此需要拷贝数据！
        m_MessageCallback(shared_from_this(), m_RecvBuf);

        // FIXME 如果m_MessageCallback的实际任务不在本线程运行，而是在其他线程（如线程池中的线程），那么该会导致其他线程中的buf已被Reset
    } else {
        //! 允许适当扩容，但是用户发送的数据大于 recv_bytes_max，则连接异常，关闭之
        if(m_RecvBuf.GetMaxsize() > g_app_config->GetValue().recv_bytes_max()) {
            HandleClose();
            YLOG_WARN("<{}>TcpConnection::HandleRead(): 用户连接发送数据过多<{}+{}>{}>，关闭连接",
                      m_RecvBuf.GetDataSize(), (size_t) out_nBytesRead, m_RecvBuf.GetMaxsize(), m_Socket->GetFD())
        }
    }
}



void TcpConnection::HandleClose() {
    m_ioLoop->AssertInLoopingThread(__FILE__, __LINE__);

    //! 用户要关闭连接时，并不直接Close，而是先Shundown，等到一定时间之后再统一Close
    switch (m_ConnectionState)
    {
        case eConnected:
        case eShutdown:
            ShutdownInLoop();
            break;
        default:
            break;
    }
}

void TcpConnection::HandleError() {
    m_ioLoop->AssertInLoopingThread(__FILE__, __LINE__);

    HandleClose();
}

SocketApiWrapper::SocketResult TcpConnection::HandleRead_ET() {
    m_ioLoop->AssertInLoopingThread(__FILE__, __LINE__);

    SocketApiWrapper::SocketResult rst;

    //! 边缘触发：只会到数据到来时触发一次可读事件，之后不管这些数据是否未被读取完，都不会再触发可读事件，因此一次事件需要一直读取套接字直到没有数据(EAGAIN)
    while(true)
    {
        //! ET读取
        m_RecvBuf.AppendDataFromSocket(m_Socket, g_app_config->GetValue().recv_bytes_one(), rst);

        YLOG_TRACE("<{}>TcpConnection::HandleRead_ET(): 读取<{}>字节", m_Socket->GetFD(), rst.Result())

        // 数据读取完毕
        if (rst.HasError())
        {
            const auto err = rst.ErrorCode();
            YLOG_ERROR("<{}>TcpConnection::HandleRead_:ET(): recv() error: {}", m_Socket->GetFD(), rst.GetErrorInfo())
            switch (err) {
                //! 接收正常结束： EAGAIN，表示已无数据可recv，即数据全部recv完毕
                case SocketError::eAgain:
                    rst = {rst.Result(), 0};
                    break;
                //! 再次尝试：recv被信号打断，应重试
                case SocketError::eInterrupted:
                    continue;
                case SocketError::eConnectionReset:
                case SocketError::eConnectionAborted:
                case SocketError::eNotConnected:
                    YLOG_INFO("<{}>TcpConnection::HandleRead_ET(): 用户连接关闭，关闭用户连接", m_Socket->GetFD());
                    HandleClose();
                    break;
                default:
                    YLOG_INFO("发生错误<{}>", rst.GetErrorInfo());
                    HandleError();
                    break;
            }

            //! 结束while循环
            break;
        }
        else
        {
            const auto nBytesRecv = rst.Result();

            // 连接关闭请求
            if (nBytesRecv == 0) {
                YLOG_INFO("<{}>TcpConnection::HandleRead_ET(): 用户请求关闭，关闭用户连接", m_Socket->GetFD());
                HandleClose();
                break; //! FIXED_BUG 不要漏了，因为断开连接会回收userdata，Reset之，如果再循环一次会导致bad fd错误
            }

            //! 接收正常结束：若本次接收的数据小于一次最多能接收的数据，说明本次接收是本批recv()的最后一份数据，可以直接结束本次HandleRead_ET，节省了一次调用recv的时间
            if (static_cast<size_t>(nBytesRecv) < g_app_config->GetValue().recv_bytes_one()) {
                break;
            }
        }
    }
    return rst;
}

SocketApiWrapper::SocketResult TcpConnection::HandleRead_LT() {
    SocketApiWrapper::SocketResult rst;
    m_RecvBuf.AppendAllDataFromSocket(m_Socket, rst);

    if (rst.HasError()) {
        YLOG_ERROR("<{}>TcpConnection::HandleRead_:LT(): recv() error: {}", m_Socket->GetFD(), rst.GetErrorInfo())
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
            case SocketError::eConnectionReset:
            case SocketError::eConnectionAborted:
            case SocketError::eNotConnected:
                YLOG_INFO("<{}>TcpConnection::HandleRead_LT(): 用户连接关闭，关闭用户连接", m_Socket->GetFD());
                HandleClose();
                break;
            default:
                YLOG_INFO("发生错误<{}>", rst.GetErrorInfo());
                HandleError();
                break;
        }
    } else {
        const auto nBytesRecv = rst.Result();

        // 连接关闭请求
        if(nBytesRecv == 0) {
            YLOG_INFO("<{}>TcpConnection::HandleRead_LT(): 用户请求关闭，关闭用户连接", m_Socket->GetFD())
            HandleClose();
        }
    }


    return rst;
}






}
