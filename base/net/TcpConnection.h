#ifndef LINUXGAMESERVER_TCPCONNECTION_H
#define LINUXGAMESERVER_TCPCONNECTION_H

#include <memory>
#include "net_definations.h"
#include "IPAddress.h"
#include "Buffer.h"
#include "Timestamp.h"

namespace yy::net {

class Channel;
class EventLoop;
class Socket;


class TcpConnection: public std::enable_shared_from_this<TcpConnection> {
/*
 * Disconnected ━━▶ Connecting ━━▶ Connected ━━▶ Shutdown
 *      ▲                                            ┃
 *      ┃                                            ┃
 *      ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
 * */
    enum E_ConnectionState {
        eDisconnected = 0,
        eConnecting   = 1,
        eConnected    = 2, //! 处于Connected的连接在更上层会有更多的子状态
        eShutdown     = 3,
    };

public:
    ///@brief Acceptor接受用户连接后，在NewConnection回调函数（由TcpServer定义）中创建的数据结构
    TcpConnection(std::string name, EventLoop *loop, SocketApiWrapper::socket_t sockfd,
                  IPAddress::ptr localAddr, IPAddress::ptr peerAddr);

    ~TcpConnection();

    //! 将待发送数据message添加至输出缓冲中（如果输出缓冲为空，则直接发送，无需等待事件触发）
    void Send(const void * buf, size_t len);
    void Send(const Buffer & buf);
    void Send(const std::string_view & message);
    void Send(const google::protobuf::Message & message);

    /// @brief 关闭用户连接，但是不回收文件描述符，仍保留系统分配的套接字的资源(如缓存)，适合用户掉线可能马上再连接的情况。
    void Shutdown();

    /// @brief 关闭用户连接，而且回收文件描述符，释放用户套接字的资源，适合用户连接确认已经关闭的情况。
    /// Shutdown之后，如果用户连接确实需要关闭，则需要调用Close来释放套接字资源
    void Close();

    ///@brief 因为需要设置状态转换，所以定义在这里
    // called when TcpServer accepts a new connection
    void ConnectionEstablished();
    // called when TcpServer has removed me from its map
    void ConnectionDestroyed();


    /*! GETTER !*/
    const std::string &    GetName()      const { return m_Name; }
    EventLoop *            GetLoop()      const { return m_Loop; }
    const IPAddress::ptr & GetLocalAddr() const { return m_LocalAddr; }
    const IPAddress::ptr & GetPeerAddr()  const { return m_PeerAddr; }
    const Buffer &         GetSendBuf()   const { return m_SendBuf; }
    const Buffer &         GetReadBuf()   const { return m_RecvBuf; }
    Timestamp              GetConnectedTime() const { return m_ConnectedTime; }
    Timestamp              GetShudownTime()   const { return m_ShudownTime; }
    Timestamp              GetHeartTime()     const { return m_HeartTime; }
    uint8_t                GetXorCode() const { return m_XorCode; }
    int                    GetSocketFD()  const ;
    bool  IsConnected()    { return m_ConnectionState == eConnected; }
    bool  IsConnecting()   { return m_ConnectionState == eConnecting; }
    bool  IsDisconnected() { return m_ConnectionState == eDisconnected; }
    bool  IsShutdown()     { return m_ConnectionState == eShutdown; }

    /*! SETTER !*/
    void SetConnectionEstablishedCallback  (F_ConnectionEstablishedCallback cb)   { m_ConnectionEstablishedCallback = cb; }
    // void SetConnectionDestroyedCallback    (F_ConnectionDestroyedCallback cb)     { m_ConnectionDestroyedCallback = cb; }
    void SetMessageCallback                (F_MessageCallback cb)                 { m_MessageCallback = cb; }
    void SetConnectionWriteCompleteCallback(F_ConnectionWriteCompleteCallback cb) { m_ConnectionWriteCompleteCallback = cb; }
    void SetConnectionCloseCallback        (F_ConnectionCloseCallback cb)         { m_ConnectionCloseCallback = cb; }
    void SetConnectionShutdownCallback     (F_ConnectionShutdownCallback cb)      { m_ConnectionShutdownCallback = cb; }



    void SetXorCode(uint8_t xorCode) { m_XorCode = xorCode; }

private:
    void SetState(E_ConnectionState state) { m_ConnectionState = state; }

    void HandleRead();     // 将套接字的数据接收到RecvBuf中
    bool HandleRead_ET();  // 将套接字的数据接收到RecvBuf中
    void HandleWrite();    // 将SendBuf中的数据全部发送出去
    void HandleClose();    // 关闭套接字
    void HandleError();    // 处理错误

    void SendInLoop(const std::string_view &buf);
    void ShutdownInLoop();
    void CloseInLoop();

    bool CanClose() { return IsConnected() or IsShutdown(); }
    bool CanShutdown() { return !IsShutdown() and IsConnected(); }

private:
    std::string                    m_Name;
    EventLoop *                    m_Loop;
    std::unique_ptr<Socket>        m_Socket;
    std::unique_ptr<Channel>       m_Channel;
    uint8_t                        m_XorCode;
    std::atomic<E_ConnectionState> m_ConnectionState;

    const IPAddress::ptr m_LocalAddr;
    const IPAddress::ptr m_PeerAddr;

    F_ConnectionEstablishedCallback      m_ConnectionEstablishedCallback;   //! 需要及时响应，立即执行
    // F_ConnectionDestroyedCallback        m_ConnectionDestroyedCallback;     //! 需要及时响应，立即执行
    F_MessageCallback                    m_MessageCallback;                 //! 需要及时响应，立即执行
    F_ConnectionWriteCompleteCallback    m_ConnectionWriteCompleteCallback; //! 可不立即执行
    F_ConnectionCloseCallback            m_ConnectionCloseCallback;         //! 可不立即执行
    F_ConnectionShutdownCallback         m_ConnectionShutdownCallback;

    //! 用户不会直接操作SendBuf，而是用Send间接操作，因此保证线程安全
    Buffer m_SendBuf;

    ///! 用户应保证只在MessageCallback回调的调用者线程（即本TcpConnection所在线程）中操作RecvBuf，请不要传递给其它线程，否则RecvBuf不是线程安全的。
    Buffer m_RecvBuf;
    std::vector<char> m_TempRecvBuf;


    Timestamp m_ConnectedTime;
    Timestamp m_ShudownTime;
    Timestamp m_HeartTime;
};

}
#endif //LINUXGAMESERVER_TCPCONNECTION_H
