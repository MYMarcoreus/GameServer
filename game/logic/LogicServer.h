#ifndef GAMESERVER_LOGICSERVER_H
#define GAMESERVER_LOGICSERVER_H

#include "IServer.h"
#include "TcpServer.h"
#include "UdpServer.h"
#include "ProtobufTcpCodec.h"
#include "ProtobufUdpCodec.h"
#include "ProtobufDispatcher.h"
#include "net_definations.h"
#include "ThreadPool.h"
#include <future>

#include "MySqlPool.h"


namespace yy::protocol::core {
class HeartBody;
class C2SSecurityBody;
class C2SUdpPortRegister;
}

using yy::core::UserConnectionPtr;
using yy::core::MessagePtr;

namespace yy::app::logic {


class LogicServer final: public core::IServer{
    using HeartPtr    = std::shared_ptr<yy::protocol::core::HeartBody> ;
    using C2SSecurityPtr = std::shared_ptr<yy::protocol::core::C2SSecurityBody> ;
    using C2SUdpPortRegisterPtr = std::shared_ptr<yy::protocol::core::C2SUdpPortRegister> ;

public:
    LogicServer(yy::net::EventLoop* accpetorLoop, const yy::net::IPAddressPtr& listenAddr);
    ~LogicServer() override;

    /// @brief Start Listen & IOLoop
    virtual void Start() override;

    /// @brief 结束服务器
    virtual void Stop() override;

    /* * 根据用户连接名称来寻找用户基础数据 * */
    virtual UserConnectionPtr   FindUser(uint64_t conn_id) override;
    virtual void                DelUser (uint64_t conn_id) override;
    virtual void                AddUser (uint64_t conn_id, const UserConnectionPtr & userdata) override;

    virtual bool IsRunning() const override { return m_tcpServer.IsRunning(); }
    virtual const config::AppXmlConfig & GetAppConfig() override { return m_appConfigvar->GetValue(); }

    /* * 由业务层定义并传入 * */
    virtual void SetNotifier_Security  (const F_Notifier cb) override { m_NotifierSecurity   = cb; }
    virtual void SetNotifier_DisConnect(const F_Notifier cb) override { m_NotifierDisconnect = cb; }
    virtual void SetNotifier_Command(const F_NotifierCommand cb) override { m_NotifierCommand = cb; }

    /* * 在Acceptor中运行定时器（其实可以新建一个定时器线程） * */
    virtual net::TimerID RunAt(net::Timestamp time, net::F_TaskCallback cb) override;
    virtual net::TimerID RunAfter(net::Microseconds delay, net::F_TaskCallback cb) override;
    virtual net::TimerID RunEvery(net::Microseconds interval, net::F_TaskCallback cb) override;
    virtual void CancelTimer(net::TimerID timerid) override;

private:
    void OnUnknownTcpMessage(const net::TcpConnectionPtr &conn, const MessagePtr& message);
    void OnUnknownUdpMessage(const net::UdpSessionPtr &conn, const MessagePtr& message);

    void OnConnectionEstablished(const net::TcpConnectionPtr & conn);
    void AddCheckTimer(const net::TcpConnectionPtr & conn, const UserConnectionPtr & userdata);
    void CheckHeart(const UserConnectionPtr & userdata);
    void SendXorCode(const yy::net::TcpConnectionPtr &conn);

    void OnTcpHeart(const yy::net::TcpConnectionPtr &conn, const HeartPtr & message);
    void OnUdpHeart(const yy::net::UdpSessionPtr &conn, const HeartPtr & message);
    void OnSecurity(const net::TcpConnectionPtr & conn, const C2SSecurityPtr & message);
    void OnC2SUdpPortRegister(const net::TcpConnectionPtr & conn, const C2SUdpPortRegisterPtr & message);

    void AfterShutdownConnection(const yy::net::TcpConnectionPtr &conn);

private:
    yy::config::ConfigVar<yy::config::AppXmlConfig>::ptr    m_appConfigvar; // 用于获取配置项
    yy::net::EventLoop *                                    m_accpetorLoop;

    yy::net::TcpServer                                      m_tcpServer;
    core::ProtobufDispatcher<yy::net::TcpConnectionPtr>           m_tcpDispatcher; // 处理下层(net层)分发传来的无法处理的消息
    core::ProtobufTcpCodec                                        m_tcpCodec;

    yy::net::UdpServer                                      m_udpServer;
    core::ProtobufDispatcher<yy::net::UdpSessionPtr>              m_udpDispatcher; // 处理下层(net层)分发传来的无法处理的消息
    core::ProtobufUdpCodec                                        m_udpCodec;

    std::atomic<size_t>                                     m_numSecurity; //安全连接数
    /* 这几个回调函数由业务层实现，然后通过对应的set方法传入设置 */
    F_Notifier        m_NotifierSecurity;    // 用户安全验证通过后，执行业务层回调函数
    F_Notifier        m_NotifierDisconnect;  // 用户连接断开后，执行业务层回调函数
    F_NotifierCommand m_NotifierCommand;

    std::mutex                                          m_usersMutex;
    std::unordered_map<uint64_t , UserConnectionPtr> m_users;
    std::vector<uint64_t> m_closeUsers;

    std::unique_ptr<yy::core::MySqlPool> m_mysql_pool;
};

}

#endif
