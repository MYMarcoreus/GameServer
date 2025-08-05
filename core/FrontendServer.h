#pragma once

#include "IServer.h"
#include "TcpServer.h"
#include "UdpServer.h"
#include "ProtobufTcpCodec_Cmd.h"
#include "ProtobufUdpCodec_Cmd.h"
#include "ProtobufDispatcher.h"
#include "net_definations.h"
#include "core_definations.h"
#include "ThreadPool.h"


namespace yy::protocol::core {
class HeartBody;
class SecurityCheckReq;
class UdpPortRegisterReq;
}


namespace yy::core {

class FrontendServer final: public IServer{
    using HeartPtr    = std::shared_ptr<protocol::core::HeartBody> ;
    using C2SSecurityPtr = std::shared_ptr<protocol::core::SecurityCheckReq> ;
    using UdpPortRegisterReqPtr = std::shared_ptr<protocol::core::UdpPortRegisterReq> ;

public:
    FrontendServer(net::EventLoop* accpetorLoop, const net::IPAddressPtr& tcp_addr, const net::IPAddressPtr& udp_addr);
    ~FrontendServer() override;

    /// @brief Start Listen & IOLoop
    void Start(const net::F_ThreadInitCallback& cb) override;

    /// @brief 结束服务器
    void Stop() override;

    bool IsRunning() const override { return m_tcpServer.IsRunning(); }
    const config::AppXmlConfig & GetAppConfig() override { return m_appConfigvar->GetValue(); }
    auto GetTcpListenAddr() const -> net::IPAddressPtr override { return m_tcpServer.GetListenAddr(); }

    /* * 由业务层定义并传入 * */
    void SetNotifier_Security  (const F_Notifier cb) override { m_NotifierSecurity   = cb; }
    void SetNotifier_DisConnect(const F_Notifier cb) override { m_NotifierDisconnect = cb; }
    void SetNotifier_Command(const F_NotifierCommand cb) override { m_NotifierCommand = cb; }

    /* * 在Acceptor中运行定时器（其实可以新建一个定时器线程） * */
    net::TimerID RunAt(net::Timestamp time, net::F_TaskCallback cb) override;
    net::TimerID RunAfter(net::Microseconds delay, net::F_TaskCallback cb) override;
    net::TimerID RunEvery(net::Microseconds interval, net::F_TaskCallback cb) override;
    void CancelTimer(net::TimerID timerid) override;

private:
    /* * 根据用户连接名称来寻找用户基础数据 * */
    UserConnectionPtr   FindUser(uint64_t conn_id);
    void                DelUser (uint64_t conn_id);
    void                AddUser (uint64_t conn_id, const UserConnectionPtr & userdata);

    void OnUnknownTcpMessage(const net::TcpConnectionPtr &conn, const MessagePtr& message);
    void OnUnknownUdpMessage(const net::UdpSessionPtr &sess, const MessagePtr& message);

    //Region
    void OnConnectionEstablished(const net::TcpConnectionPtr & conn);
    void AddCheckTimer(const net::TcpConnectionPtr & conn, const UserConnectionPtr & userdata);
    void CheckHeart(const UserConnectionPtr & userdata);
    void SendXorCode(const net::TcpConnectionPtr &conn);

    void OnConnectionShutdown(const net::TcpConnectionPtr &conn);
    //End

    void OnTcpHeart(const net::TcpConnectionPtr &conn, const HeartPtr & message);
    void OnUdpHeart(const net::UdpSessionPtr &conn, const HeartPtr & message);
    void OnSecurity(const net::TcpConnectionPtr & conn, const C2SSecurityPtr & message);
    void OnUdpPortRegisterRequest(const net::TcpConnectionPtr & conn, const UdpPortRegisterReqPtr & message);


private:
    net::EventLoop *                                    m_accpetorLoop;
    config::ConfigVar<config::AppXmlConfig>::ptr        m_appConfigvar; // 用于获取配置项

    net::TcpServer                                      m_tcpServer;
    ProtobufDispatcher<net::TcpConnectionPtr>           m_tcpDispatcher; // 处理下层(net层)分发传来的无法处理的消息
    ProtobufTcpCodec                                    m_tcpCodec;

    net::UdpServer                                      m_udpServer;
    ProtobufDispatcher<net::UdpSessionPtr>              m_udpDispatcher; // 处理下层(net层)分发传来的无法处理的消息
    ProtobufUdpCodec                                    m_udpCodec;

    /* 这几个回调函数由业务层实现，然后通过对应的set方法传入设置 */
    F_Notifier        m_NotifierSecurity;    // 用户安全验证通过后，执行业务层回调函数
    F_Notifier        m_NotifierDisconnect;  // 用户连接断开后，执行业务层回调函数
    F_NotifierCommand m_NotifierCommand;

    util::RWMutex                                    m_usersMutex;
    std::unordered_map<uint64_t, UserConnectionPtr>  m_users;
};

}
