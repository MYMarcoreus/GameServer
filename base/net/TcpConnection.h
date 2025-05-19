#ifndef LINUXGAMESERVER_TCPCONNECTION_H
#define LINUXGAMESERVER_TCPCONNECTION_H

#include "net_definations.h"
#include "IPAddress.h"
#include "Timestamp.h"

#include <memory>

namespace yy::net {

class IOChannel;
class EventLoop;
class Socket;

class TcpConnection: public std::enable_shared_from_this<TcpConnection> {
/*
 * Disconnected ━━▶ Connecting ━━▶ Connected ━━▶ Shutdown
 *      ▲                                            ┃
 *      ┃                                            ┃
 *      ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
 *             执行m_ConnectionDestroyedCallback
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

    ///Region 发送TCP数据：将待发送数据message添加至输出缓冲中（如果输出缓冲为空，则直接发送，无需等待事件触发）
    void SendTCP(const void * buf, size_t len);
    void SendTCP(const std::string_view & message);
    ///End

    /// @brief 关闭用户连接，但是不回收文件描述符，仍保留系统分配的套接字的资源(如缓存)
    void Shutdown();


    ///@brief 因为需要设置状态转换，所以定义在这里
    // 在m_ConnectionEstablishedCallback(TcpServer上层传入)之前调用，此时map新增一个成员
    void ConnectionEstablished();
    // 在m_ConnectionCloseCallback(TcpServer::RemoveConnection)之后调用，此时map移除一个成员
    void ConnectionDestroyed();


    ///Region GETTER
    const std::string &         GetName()          const { return m_name; }
    EventLoop *                 GetIOLoop()        const { return m_ioLoop; }
    const IPAddress::ptr &      GetLocalAddr()     const { return m_localAddr; }
    const IPAddress::ptr &      GetPeerAddr()      const { return m_peerAddr; }
    Timestamp                   GetConnectedTime() const { return m_connectedTime; }
    Timestamp                   GetShudownTime()   const { return m_shudownTime; }
    Timestamp                   GetHeartTime()     const { return m_heartTime; }
    uint8_t                     GetXorCode()       const { return m_xorCode; }
    SocketApiWrapper::socket_t  GetSocketFD()      const ;
    bool  IsConnected()    { return m_connectionState == eConnected; }
    bool  IsConnecting()   { return m_connectionState == eConnecting; }
    bool  IsDisconnected() { return m_connectionState == eDisconnected; }
    bool  IsShutdown()     { return m_connectionState == eShutdown; }
    ///End

    ///Region SETTER
    void SetConnectionEstablishedCallback  (F_ConnectionEstablishedCallback cb)   { m_ConnectionEstablishedCallback = cb; }
    void SetConnectionDestroyedCallback    (F_ConnectionDestroyedCallback cb)     { m_ConnectionDestroyedCallback = cb; }
    void SetMessageCallback                (F_TcpMessageCallback cb)              { m_MessageCallback = cb; }
    void SetConnectionWriteCompleteCallback(F_ConnectionWriteCompleteCallback cb) { m_ConnectionWriteCompleteCallback = cb; }
    void SetConnectionCloseCallback        (F_ConnectionCloseCallback cb)         { m_ConnectionCloseCallback = cb; }
    void SetConnectionShutdownCallback     (F_ConnectionShutdownCallback cb)      { m_ConnectionShutdownCallback = cb; }
    void SetXorCode(uint8_t xorCode) { m_xorCode = xorCode; }
    ///End

private:
    void SetState(E_ConnectionState state) { m_connectionState = state; }

    void HandleRead();     // 将套接字的数据接收到RecvBuf中
    SocketApiWrapper::SocketResult HandleRead_ET();  // 将套接字的数据接收到RecvBuf中
    SocketApiWrapper::SocketResult HandleRead_LT();  // 将套接字的数据接收到RecvBuf中
    void HandleWrite();    // 将SendBuf中的数据全部发送出去
    void HandleClose();    // 关闭套接字
    void HandleError();    // 处理错误

    void SendTCPInLoop(const std::string_view &buf);
    void ShutdownInLoop();

    bool CanShutdown() { return !IsShutdown() and IsConnected(); }

    bool CanIO() { return IsConnected(); }

private:
    std::string                      m_name;
    EventLoop *                      m_ioLoop;
    std::unique_ptr<Socket>          m_socket;
    std::unique_ptr<IOChannel>       m_channel;
    uint8_t                          m_xorCode;
    std::atomic<E_ConnectionState>   m_connectionState;

    const IPAddress::ptr m_localAddr;
    const IPAddress::ptr m_peerAddr;

    F_ConnectionEstablishedCallback      m_ConnectionEstablishedCallback;   //! 需要及时响应，立即执行
    F_ConnectionDestroyedCallback        m_ConnectionDestroyedCallback;     //! 需要及时响应，立即执行
    F_TcpMessageCallback                 m_MessageCallback;                 //! 需要及时响应，立即执行
    F_ConnectionWriteCompleteCallback    m_ConnectionWriteCompleteCallback; //! 可不立即执行
    F_ConnectionCloseCallback            m_ConnectionCloseCallback;         //! 可不立即执行
    F_ConnectionShutdownCallback         m_ConnectionShutdownCallback;

    //! 用户不会直接操作SendBuf，而是用Send间接操作，因此保证线程安全
    std::unique_ptr<Buffer> m_sendBuf;
    ///! 用户应保证只在MessageCallback回调的调用者线程（即本TcpConnection所在线程）中操作RecvBuf，请不要传递给其它线程，否则RecvBuf不是线程安全的。
    std::unique_ptr<Buffer> m_recvBuf;

    Timestamp m_connectedTime;
    Timestamp m_shudownTime;
    Timestamp m_heartTime;
};

}
#endif //LINUXGAMESERVER_TCPCONNECTION_H

