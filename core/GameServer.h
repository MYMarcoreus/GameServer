#ifndef GAMESERVER_GAMESERVER_H
#define GAMESERVER_GAMESERVER_H

#include "IServer.h"
#include "Singleton.h"
#include "TcpServer.h"
#include "codec/ProtobufCodec.h"
#include "codec/ProtobufDispatcher.h"
#include "net_definations.h"
#include "ThreadPool.h"
#include <future>


namespace yy::protocol::core {
class HeartBody;
class SecurityBody;
}


namespace yy::core {


class GameServer final: public IServer{
    using HeartPtr    = std::shared_ptr<yy::protocol::core::HeartBody> ;
    using SecurityPtr = std::shared_ptr<yy::protocol::core::SecurityBody> ;

public:
    GameServer(yy::net::EventLoop* loop, yy::net::IPAddressPtr listenAddr);
    ~GameServer() override;

    /// @brief Start Listen & IOLoop
    virtual void Start() override;

    /// @brief 结束服务器
    virtual void Stop() override;

    virtual UserBaseDataPtr FindUser(const std::string & conn_name) override;
    virtual void            DelUser(const std::string & conn_name) override;
    virtual void            AddUser(const std::string & conn_name, const UserBaseDataPtr & userdata) override;

    virtual bool IsRunning() const override { return m_server.IsRunning(); }

    virtual const config::AppXmlConfig & GetAppConfig() override { return m_app_configvar->GetValue(); }

    /* 在实现类中定义四个回调函数成员，下面这四个函数将会设置其对应的回调函数，而回调函数将由业务层定义并传入 */
    virtual void SetNotifier_Security  (F_Notifier cb) override { m_notifier_security   = cb; }
    virtual void SetNotifier_DisConnect(F_Notifier cb) override { m_notifier_disconnect = cb; }
    virtual void SetNotifier_Command(F_NotifierCommand cb) override { m_notifier_command = cb; }

    // template<typename T>
    // void RegisterMessageCallback(const CallbackT<T>::ProtobufMessageTCallback &callback) {
    //     m_dispatcher.RegisterMessageCallback<T>(callback);
    // }

    virtual net::TimerID RunAt(net::Timestamp time, net::F_TaskCallback cb) override;
    virtual net::TimerID RunAfter(net::Microseconds delay, net::F_TaskCallback cb) override;
    virtual net::TimerID RunEvery(net::Microseconds interval, net::F_TaskCallback cb) override;
    virtual void CancelTimer(net::TimerID timerid) override;


private:
    void OnUnknownMessage(const net::TcpConnectionPtr & userdata, const MessagePtr& message);

    void OnConnectionEstablished(const net::TcpConnectionPtr & userdata);
    void AddCheckTimer(const net::TcpConnectionPtr & conn, const UserBaseDataPtr & userdata);
    void CheckHeart(const UserBaseDataPtr & conn);
    void SendXorCode(const yy::net::TcpConnectionPtr &conn);

    void OnHeart(const net::TcpConnectionPtr & conn, const HeartPtr & message);
    void OnSecurity(const net::TcpConnectionPtr & conn, const SecurityPtr & message);

    void AfterShutdownConnection(const yy::net::TcpConnectionPtr &conn);
private:
    yy::net::EventLoop *        m_accpetorLoop;
    yy::net::TcpServer          m_server;
    ProtobufCodec               m_codec;
    ProtobufDispatcher<yy::net::TcpConnectionPtr>        m_dispatcher;

    yy::config::ConfigVar<yy::config::AppXmlConfig>::ptr    m_app_configvar; // 用于获取配置项
    std::atomic<size_t> m_num_security; //安全连接数

    /* 这几个回调函数由业务层实现，然后通过对应的set方法传入设置 */
    F_Notifier m_notifier_security;    // 用户安全验证通过后，执行业务层回调函数
    F_Notifier m_notifier_disconnect;  // 用户连接断开后，执行业务层回调函数
    F_NotifierCommand m_notifier_command;

    std::mutex                                        m_users_mutex;
    std::unordered_map<std::string , UserBaseDataPtr> m_users;
    std::vector<std::string> m_closeUsers;

};

}

#endif //GAMESERVER_GAMESERVER_H
