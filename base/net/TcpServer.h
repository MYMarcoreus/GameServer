#ifndef LINUXGAMESERVER_TCPSERVER_H
#define LINUXGAMESERVER_TCPSERVER_H

#include "net_definations.h"
#include "IPAddress.h"
#include "AppXmlConfig.h"
#include "ThreadSafeQueue.hpp"

#include <atomic>
#include <map>

namespace yy::net {

class Acceptor;
class EventLoopThreadPool;

class TcpServer {
public:
    ///@param
    ///@param
    ///@param
    ///@note 注意其实不要将线程数量作为构造函数的参数，不要在构造函数里构造Loop线程池，因为我们需要再Start中才一个一个创建线程，
    /// 而非在构造函数中（即使在构造函数中没有创建线程，但为了语义上歧义少点，请不要这么做）
    TcpServer(EventLoop *acceptorLoop, IPAddress::ptr listenAddr, bool reusePort) noexcept;
    ~TcpServer();

    ///@brief 启动连接池并开启监听套接字
    void Start(int threadNum, Milliseconds ioWaitTimeout, F_ThreadInitCallback cb = F_ThreadInitCallback());

    void Stop();

    //! TcpConnection回调，由TcpServer的上层定义并实现
    void SetConnectionEstablishedCallback(F_ConnectionEstablishedCallback cb) { m_ConnectionEstablishedCallback = cb; };
    // void SetConnectionDestroyedCallback(F_ConnectionDestroyedCallback cb) { m_ConnectionDestroyedCallback = cb; };
    void SetConnectionWriteCompleteCallback(F_ConnectionWriteCompleteCallback cb) { m_ConnectionWriteCompleteCallback = cb; };
    void SetConnectionShutdownCallback     (F_ConnectionShutdownCallback cb)      { m_ConnectionShutdownCallback = cb; }
    void SetCloseSocketsCallback(F_CloseShutdownConnectionsCallback cb);


    /* ! 注意：当使用线程池时，不要把recvBuf的引用或指针作为参数传递给另一线程（如线程池中的线程），
       ! MessageCallback需在的调用者线程中（即TcpConnection对象所在线程，即在onMessage中）完成对recvBuf数据的拷贝，
       ! 否则可能在成recvBuf的线程不安全 */
    void SetMessageCallback(F_MessageCallback cb) { m_MessageCallback = cb; };

    size_t GetConnectionsCount() { return m_NumConnect; }

    bool IsRunning() const { return m_IsStarted; }

    EventLoop * GetAcceptorLoop() const { return m_AcceptorLoop; }

private:
    //! Acceptor回调
    void HandleNewConnection(SocketApiWrapper::socket_t sockfd, IPAddressPtr peerAddr);

    void RemoveConnection(const TcpConnectionPtr &conn);
    void RemoveConnectionInLoop(TcpConnectionPtr conn); //! 不能是const引用

    void InitLog();

    void HandleSignal();
private:

    EventLoop *                          m_AcceptorLoop;
    std::unique_ptr<Acceptor>            m_Acceptor;
    std::unique_ptr<EventLoopThreadPool> m_IOThreadPool;
    std::unique_ptr<class SignalManager> m_SignalManager;

    std::atomic<bool> m_IsStarted{false};
    uint64_t          m_NextConnID{0};
    std::atomic<size_t> m_NumConnect{0};  //当前连接数


    F_ConnectionEstablishedCallback      m_ConnectionEstablishedCallback;
 // F_ConnectionDestroyedCallback        m_ConnectionDestroyedCallback;
    F_ConnectionWriteCompleteCallback    m_ConnectionWriteCompleteCallback;
    F_MessageCallback                    m_MessageCallback;
 // F_ConnectionCloseCallback            m_ConnectionCloseCallback;  // 不允许让用户指定close回调
    F_ConnectionShutdownCallback         m_ConnectionShutdownCallback;
    // F_CloseShutdownConnectionsCallback   m_CloseSocketsCallback;

    config::ConfigVar<config::AppXmlConfig>::ptr m_AppConfigVar; // 用于获取配置项
    std::map<std::string , TcpConnectionPtr> m_ConnectionMap;


};



}
#endif //LINUXGAMESERVER_TCPSERVER_H
