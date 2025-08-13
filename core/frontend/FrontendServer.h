#pragma once

#include "IServer.h"
#include "ProtobufDispatcher.h"
#include "net_definations.h"
#include "../core_definations.h"
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

    bool IsRunning() const override;
    auto GetAppConfig() -> const config::AppXmlConfig& override;
    auto GetTcpListenAddr() const -> net::IPAddressPtr override;

    //Region 业务层定义的回调函数
    void SetNotifier_Security  (const F_Notifier cb) override { m_NotifierSecurity   = cb; }
    void SetNotifier_DisConnect(const F_Notifier cb) override { m_NotifierDisconnect = cb; }
    void SetNotifier_Command(const F_NotifierCommand cb) override { m_NotifierCommand = cb; }
    //End

    //Region 定时器相关：在Acceptor中运行定时器
    net::TimerID RunAt(net::Timestamp time, net::F_TaskCallback cb) override;
    net::TimerID RunAfter(net::Microseconds delay, net::F_TaskCallback cb) override;
    net::TimerID RunEvery(net::Microseconds interval, net::F_TaskCallback cb) override;
    void CancelTimer(net::TimerID timerid) override;
    //End

private:
    //Region 用户连接管理函数
    UserConnectionPtr   FindUser(uint64_t conn_id);
    void                DelUser (uint64_t conn_id);
    void                AddUser (uint64_t conn_id, const UserConnectionPtr & userdata);
    //End

    ///Region 调用m_NotifierCommand，将消息传递至业务层
    void OnUnknownTcpMessage(const net::TcpConnectionPtr &conn, const MessagePtr& message);
    void OnUnknownUdpMessage(const net::UdpSessionPtr &sess, const MessagePtr& message);
    //End

    //Region 连接建立后的协议验证部分
    void OnConnectionEstablished(const net::TcpConnectionPtr & conn);
    void AddCheckTimer(const net::TcpConnectionPtr & conn, const UserConnectionPtr & userdata);
    void CheckHeart(const UserConnectionPtr & userdata);
    void SendXorCode(const net::TcpConnectionPtr &conn);
    void OnTcpHeart(const net::TcpConnectionPtr &conn, const HeartPtr & message);
    void OnUdpHeart(const net::UdpSessionPtr &conn, const HeartPtr & message);
    void OnSecurity(const net::TcpConnectionPtr & conn, const C2SSecurityPtr & message);
    void OnUdpPortRegisterRequest(const net::TcpConnectionPtr & conn, const UdpPortRegisterReqPtr & message);
    //End

    void OnConnectionShutdown(const net::TcpConnectionPtr &conn);

private:
    net::EventLoop *                                            m_accpetorLoop;
    std::shared_ptr<config::ConfigVar<config::AppXmlConfig>>    m_appConfigvar; // 用于获取配置项

    std::unique_ptr<net::TcpServer>                     m_tcpServer;
    ProtobufDispatcher<net::TcpConnectionPtr>           m_tcpDispatcher; // 处理下层(net层)分发传来的无法处理的消息
    std::unique_ptr<ProtobufTcpCodec>                   m_tcpCodec;

    std::unique_ptr<net::UdpServer>                     m_udpServer;
    ProtobufDispatcher<net::UdpSessionPtr>              m_udpDispatcher; // 处理下层(net层)分发传来的无法处理的消息
    std::unique_ptr<ProtobufUdpCodec>                   m_udpCodec;

    /* 这几个回调函数由业务层实现，然后通过对应的set方法传入设置 */
    F_Notifier        m_NotifierSecurity;    // 用户安全验证通过后，执行业务层回调函数
    F_Notifier        m_NotifierDisconnect;  // 用户连接断开后，执行业务层回调函数
    F_NotifierCommand m_NotifierCommand;

    util::RWMutex                                    m_usersMutex;
    std::unordered_map<uint64_t, UserConnectionPtr>  m_users;
};

}
