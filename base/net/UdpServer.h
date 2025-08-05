#pragma once
#include "net_definations.h"

#include <atomic>

namespace yy::net {
class EventLoopThread;

class UdpTransport;
class EventLoopThreadPool;

class UdpServer {
public:
    ///@param
    ///@param
    ///@param
    ///@note 注意其实不要将线程数量作为构造函数的参数，不要在构造函数里构造Loop线程池，因为我们需要再Start中才一个一个创建线程，
    /// 而非在构造函数中（即使在构造函数中没有创建线程，但为了语义上歧义少点，请不要这么做）
    UdpServer(EventLoop * mainLoop, const IPAddressPtr& udp_addr, bool reusePort, int32_t recv_bytes_one, int32_t send_thread_num, uint8_t init_xor_code) noexcept;
    ~UdpServer();

    void Start(int ioThreadNum, Milliseconds ioWaitTimeout);

    void Stop();

    bool IsRunning() const { return m_IsStarted; }

    /* ! 注意：当使用线程池时，不要把recvBuf的引用或指针作为参数传递给另一线程（如线程池中的线程），
       ! MessageCallback需在的调用者线程中（即TcpConnection对象所在线程，即在onMessage中）完成对recvBuf数据的拷贝，
       ! 否则可能在成recvBuf的线程不安全 */
    void SetMessageCallback(F_UdpMessageCallback cb) { m_MessageCallback = std::move(cb); };

    auto GetMainLoop() const -> EventLoop* { return m_mainLoop; }
    auto GetUdpTran() const -> UdpTransport& { return *m_udpTran; }
    auto GetRecvAddr() const -> IPAddressPtr;

private:
    void HandleNewMessage(NetBuffer & recvBuf, IPAddressPtr peerAddr);
private:
    int32_t  m_recv_bytes_one;
    int32_t  m_send_thread_num;
    uint8_t  m_init_xor_code;

    EventLoop *                             m_mainLoop;
    F_UdpMessageCallback                    m_MessageCallback;
    std::unique_ptr<EventLoopThread>        m_recvLoopThread;
    std::unique_ptr<UdpTransport>           m_udpTran;

    std::atomic<bool>   m_IsStarted{false};

};



}
