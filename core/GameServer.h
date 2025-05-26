#ifndef GAMESERVER_GAMESERVER_H
#define GAMESERVER_GAMESERVER_H

#include "IServer.h"
#include "TcpServer.h"
#include "UdpServer.h"
#include "codec/ProtobufTcpCodec.h"
#include "codec/ProtobufUdpCodec.h"
#include "codec/ProtobufDispatcher.h"
#include "net_definations.h"
#include "ThreadPool.h"
#include <future>


namespace yy::protocol::core {
class HeartBody;
class SecurityBody;
class UdpPortRegisterRequest;
}


namespace yy::core {


class GameServer final: public IServer{
    using HeartPtr    = std::shared_ptr<yy::protocol::core::HeartBody> ;
    using SecurityPtr = std::shared_ptr<yy::protocol::core::SecurityBody> ;
    using UdpPortRegisterRequestPtr = std::shared_ptr<yy::protocol::core::UdpPortRegisterRequest> ;

public:
    GameServer(yy::net::EventLoop* accpetorLoop, const yy::net::IPAddressPtr& listenAddr);
    ~GameServer() override;

    /// @brief Start Listen & IOLoop
    virtual void Start() override;

    /// @brief 结束服务器
    virtual void Stop() override;

    /* * 根据用户连接名称来寻找用户基础数据 * */
    virtual UserConnectionPtr FindUser(const std::string & conn_name) override;
    virtual void            DelUser (const std::string & conn_name) override;
    virtual void            AddUser (const std::string & conn_name, const UserConnectionPtr & userdata) override;

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

    // template<typename T>
    // void RegisterMessageCallback(const CallbackT<T>::ProtobufMessageTCallback &callback) {
    //     m_dispatcher.RegisterMessageCallback<T>(callback);
    // }

private:
    void OnUnknownTcpMessage(const net::TcpConnectionPtr &conn, const MessagePtr& message);
    void OnUnknownUdpMessage(const net::UdpSessionPtr &conn, const MessagePtr& message);

    void OnConnectionEstablished(const net::TcpConnectionPtr & conn);
    void AddCheckTimer(const net::TcpConnectionPtr & conn, const UserConnectionPtr & userdata);
    void CheckHeart(const UserConnectionPtr & userdata);
    void SendXorCode(const yy::net::TcpConnectionPtr &conn);

    void OnTcpHeart(const yy::net::TcpConnectionPtr &conn, const HeartPtr & message);
    void OnUdpHeart(const yy::net::UdpSessionPtr &conn, const HeartPtr & message);
    void OnSecurity(const net::TcpConnectionPtr & conn, const SecurityPtr & message);
    void OnUdpPortRegisterRequest(const net::TcpConnectionPtr & conn, const UdpPortRegisterRequestPtr & message);

    void AfterShutdownConnection(const yy::net::TcpConnectionPtr &conn);

private:
    yy::config::ConfigVar<yy::config::AppXmlConfig>::ptr    m_appConfigvar; // 用于获取配置项
    yy::net::EventLoop *                                    m_accpetorLoop;

    yy::net::TcpServer                                      m_tcpServer;
    ProtobufDispatcher<yy::net::TcpConnectionPtr>           m_tcpDispatcher; // 处理下层(net层)分发传来的无法处理的消息
    ProtobufTcpCodec                                        m_tcpCodec;

    yy::net::UdpServer                                      m_udpServer;
    ProtobufDispatcher<yy::net::UdpSessionPtr>              m_udpDispatcher; // 处理下层(net层)分发传来的无法处理的消息
    ProtobufUdpCodec                                        m_udpCodec;

    std::atomic<size_t>                                     m_numSecurity; //安全连接数
    /* 这几个回调函数由业务层实现，然后通过对应的set方法传入设置 */
    F_Notifier        m_NotifierSecurity;    // 用户安全验证通过后，执行业务层回调函数
    F_Notifier        m_NotifierDisconnect;  // 用户连接断开后，执行业务层回调函数
    F_NotifierCommand m_NotifierCommand;

    std::mutex                                          m_usersMutex;
    std::unordered_map<std::string , UserConnectionPtr> m_users;
    std::vector<std::string> m_closeUsers;
};

}

#endif //GAMESERVER_GAMESERVER_H

