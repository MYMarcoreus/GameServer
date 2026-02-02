#pragma once
#include "net_definations.h"
#include "IPAddress.h"
#include "AppXmlConfig.h"
#include "UnboundedLockedQueue.hpp"
#include <atomic>


namespace yy::net {

class Acceptor;
class EventLoopThreadPool;
class SignalManager;

class TcpServer {
public:
    ///@param
    ///@param
    ///@param
    ///@note 注意其实不要将线程数量作为构造函数的参数，不要在构造函数里构造Loop线程池，因为我们需要再Start中才一个一个创建线程，
    /// 而非在构造函数中（即使在构造函数中没有创建线程，但为了语义上歧义少点，请不要这么做）
    TcpServer(EventLoop *acceptorLoop, IPAddress::ptr listenAddr, bool reusePort,
        int32_t send_bytes_one, int32_t send_bytes_max,
        int32_t recv_bytes_one, int32_t recv_bytes_max, uint8_t xor_code) noexcept;
    ~TcpServer();

    ///@brief 启动连接池并开启监听套接字
    void Start(int ioThreadNum, Milliseconds ioWaitTimeout, const F_ThreadInitCallback& cb = nullptr);

    void Stop() const;

    //! TcpConnection回调，由TcpServer的上层定义并实现
    void SetConnectionEstablishedCallback  (const F_ConnectionEstablishedCallback& cb)   { m_ConnectionEstablishedCallback = cb; };
    void SetConnectionDestroyedCallback    (const F_ConnectionDestroyedCallback& cb)     { m_ConnectionDestroyedCallback = cb; };
    void SetConnectionWriteCompleteCallback(const F_ConnectionWriteCompleteCallback& cb) { m_ConnectionWriteCompleteCallback = cb; };
    void SetConnectionShutdownCallback     (const F_ConnectionShutdownCallback& cb)      { m_ConnectionShutdownCallback = cb; }
    void SetCloseSocketsCallback           (const F_CloseShutdownConnectionsCallback& cb);
    /* ! 注意：当使用线程池时，不要把recvBuf的引用或指针作为参数传递给另一线程（如线程池中的线程），
       ! MessageCallback需在的调用者线程中（即TcpConnection对象所在线程，即在onMessage中）完成对recvBuf数据的拷贝，
       ! 否则可能在成recvBuf的线程不安全 */
    void SetMessageCallback(const F_TcpMessageCallback& cb) { m_MessageCallback = cb; } // OnTcpData

    auto GetConnectionsCount() -> size_t { return m_NumConnect; }
    auto GetAcceptorLoop() const -> EventLoop* { return m_AcceptorLoop; }
    auto GetListenAddr() const -> IPAddressPtr ;
    bool IsRunning() const { return m_IsStarted.load(std::memory_order_acquire); }

private:
    //! Acceptor回调
    void HandleNewConnection(SocketApiWrapper::socket_t sockfd, IPAddressPtr peerAddr);

    void RemoveConnection(const TcpConnectionPtr &conn);
    void RemoveConnectionInLoop(TcpConnectionPtr conn); //! 不能是const引用

    void HandleSignal();
private:
    int32_t m_send_bytes_one;
    int32_t m_send_bytes_max;
    int32_t m_recv_bytes_one;
    int32_t m_recv_bytes_max;
    uint8_t m_xorCode;

    EventLoop *                          m_AcceptorLoop;
    std::unique_ptr<Acceptor>            m_Acceptor;
    std::unique_ptr<EventLoopThreadPool> m_IOThreadPool;


    std::atomic<bool>   m_IsStarted{false};
    uint64_t            m_NextConnID{1000};
    std::atomic<size_t> m_NumConnect{0};  //当前连接数


    F_ConnectionEstablishedCallback      m_ConnectionEstablishedCallback;
    F_ConnectionDestroyedCallback        m_ConnectionDestroyedCallback;
    F_ConnectionWriteCompleteCallback    m_ConnectionWriteCompleteCallback;
    F_TcpMessageCallback                 m_MessageCallback; // OnTcpData
 // F_ConnectionCloseCallback            m_ConnectionCloseCallback;  // 不允许让用户指定close回调
    F_ConnectionShutdownCallback         m_ConnectionShutdownCallback;

    std::unordered_map<uint64_t , TcpConnectionPtr> m_ConnectionMap;

#ifdef ____LINUX
    std::unique_ptr<SignalManager> m_SignalManager;
#endif
};



}
