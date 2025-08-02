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

using namespace yy::net;

namespace yy::core {


class FrontendServer final: public IServer{
    using HeartPtr    = std::shared_ptr<protocol::core::HeartBody> ;
    using C2SSecurityPtr = std::shared_ptr<protocol::core::SecurityCheckReq> ;
    using UdpPortRegisterReqPtr = std::shared_ptr<protocol::core::UdpPortRegisterReq> ;

public:
    FrontendServer(EventLoop* accpetorLoop, const IPAddressPtr& listenAddr);
    ~FrontendServer() override;

    /// @brief Start Listen & IOLoop
    void Start(const F_ThreadInitCallback& cb) override;

    /// @brief 结束服务器
    void Stop() override;

    /* * 根据用户连接名称来寻找用户基础数据 * */
    UserConnectionPtr   FindUser(uint64_t conn_id) override;
    void                DelUser (uint64_t conn_id) override;
    void                AddUser (uint64_t conn_id, const UserConnectionPtr & userdata) override;

    bool IsRunning() const override { return m_tcpServer.IsRunning(); }
    const config::AppXmlConfig & GetAppConfig() override { return m_appConfigvar->GetValue(); }
    IPAddressPtr GetListenAddr() const override { return m_listenAddr; }

    /* * 由业务层定义并传入 * */
    void SetNotifier_Security  (const F_Notifier cb) override { m_NotifierSecurity   = cb; }
    void SetNotifier_DisConnect(const F_Notifier cb) override { m_NotifierDisconnect = cb; }
    void SetNotifier_Command(const F_NotifierCommand cb) override { m_NotifierCommand = cb; }

    /* * 在Acceptor中运行定时器（其实可以新建一个定时器线程） * */
    TimerID RunAt(Timestamp time, F_TaskCallback cb) override;
    TimerID RunAfter(Microseconds delay, F_TaskCallback cb) override;
    TimerID RunEvery(Microseconds interval, F_TaskCallback cb) override;
    void CancelTimer(TimerID timerid) override;

private:
    void OnUnknownTcpMessage(const TcpConnectionPtr &conn, const MessagePtr& message);
    void OnUnknownUdpMessage(const UdpSessionPtr &sess, const MessagePtr& message);

    void OnConnectionEstablished(const TcpConnectionPtr & conn);
    void AddCheckTimer(const TcpConnectionPtr & conn, const UserConnectionPtr & userdata);
    void CheckHeart(const UserConnectionPtr & userdata);
    void SendXorCode(const TcpConnectionPtr &conn);

    void OnTcpHeart(const TcpConnectionPtr &conn, const HeartPtr & message);
    void OnUdpHeart(const UdpSessionPtr &conn, const HeartPtr & message);
    void OnSecurity(const TcpConnectionPtr & conn, const C2SSecurityPtr & message);
    void OnUdpPortRegisterRequest(const TcpConnectionPtr & conn, const UdpPortRegisterReqPtr & message);

    void AfterShutdownConnection(const TcpConnectionPtr &conn);

private:
    IPAddressPtr                                   m_listenAddr;
    config::ConfigVar<config::AppXmlConfig>::ptr   m_appConfigvar; // 用于获取配置项
    EventLoop *                                    m_accpetorLoop;

    TcpServer                                      m_tcpServer;
    ProtobufDispatcher<TcpConnectionPtr>           m_tcpDispatcher; // 处理下层(net层)分发传来的无法处理的消息
    ProtobufTcpCodec                                        m_tcpCodec;

    UdpServer                                      m_udpServer;
    ProtobufDispatcher<UdpSessionPtr>              m_udpDispatcher; // 处理下层(net层)分发传来的无法处理的消息
    ProtobufUdpCodec                                        m_udpCodec;

    std::atomic<size_t>                                     m_numSecurity; //安全连接数
    /* 这几个回调函数由业务层实现，然后通过对应的set方法传入设置 */
    F_Notifier        m_NotifierSecurity;    // 用户安全验证通过后，执行业务层回调函数
    F_Notifier        m_NotifierDisconnect;  // 用户连接断开后，执行业务层回调函数
    F_NotifierCommand m_NotifierCommand;

    util::RWMutex                                    m_usersMutex;
    std::unordered_map<uint64_t , UserConnectionPtr> m_users;
    std::vector<uint64_t> m_closeUsers;
};

}
