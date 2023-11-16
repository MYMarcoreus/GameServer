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


class GameServer: public IServer{
    using HeartPtr    = std::shared_ptr<yy::protocol::core::HeartBody> ;
    using SecurityPtr = std::shared_ptr<yy::protocol::core::SecurityBody> ;

public:
    GameServer(yy::net::EventLoop* loop, yy::net::IPAddressPtr listenAddr);
    ~GameServer() override;

    /// @brief Start Listen & IOLoop
    virtual void Start() override;

    /// @brief 结束服务器
    virtual void Stop() override {
    }

    virtual void Update() override;



    /// @brief 通过套接字文件描述符寻找用户连接数据

    virtual UserBaseDataPtr FindUser(const std::string & conn) override;
    virtual void FreeUser(const UserBaseDataPtr & userdata) override;
    virtual void AddUser(const std::string & name, const UserBaseDataPtr & userdata) override;


    virtual bool IsRunning() const override {
        return m_server.IsRunning();
    }


    virtual const config::AppXmlConfig & GetAppConfig() override { return m_app_configvar->GetValue(); }

    /* 在实现类中定义四个回调函数成员，下面这四个函数将会设置其对应的回调函数，而回调函数将由业务层定义并传入 */
    virtual void setNotifier_Security  (F_Notifier cb) override { m_notifierSecurity   = cb; }
    virtual void setNotifier_DisConnect(F_Notifier cb) override { m_notifierDisconnect = cb; }
    virtual void setNotifier_Command(F_NotifierCommand cb) override { m_notifierCommand = cb; }

    // template<typename T>
    // void RegisterMessageCallback(const CallbackT<T>::ProtobufMessageTCallback &callback) {
    //     m_dispatcher.RegisterMessageCallback<T>(callback);
    // }

    virtual net::TimerID RunAt(net::Timestamp time, net::F_TaskCallback cb) override;
    virtual net::TimerID RunAfter(net::Microseconds delay, net::F_TaskCallback cb) override;
    virtual net::TimerID RunEvery(net::Microseconds interval, net::F_TaskCallback cb) override;
    virtual void CancelTimer(net::TimerID timerid) override;


private:
    void OnUnknownMessage(yy::net::TcpConnectionPtr conn, const MessagePtr& message);

    void OnConnectionEstablished(yy::net::TcpConnectionPtr conn);
    void SendXorCode(const yy::net::TcpConnectionPtr &conn);

    void OnHeart(const yy::net::TcpConnectionPtr & conn, const HeartPtr & message);
    void OnSecurity(const yy::net::TcpConnectionPtr & conn, const SecurityPtr & message);

    void CheckDisconnections_Update(const UserBaseDataPtr & userdata);
    // void AddShutdownConnection(const yy::net::TcpConnectionPtr &conn);
    // void CheckDisconnections();

private:
    yy::net::EventLoop *        m_accpetorLoop;
    yy::net::TcpServer          m_server;
    ProtobufCodec               m_codec;
    ProtobufDispatcher<yy::net::TcpConnectionPtr>        m_dispatcher;

    yy::config::ConfigVar<yy::config::AppXmlConfig>::ptr    m_app_configvar; // 用于获取配置项
    std::atomic<size_t> m_NumSecurity; //安全连接数

    /* 这几个回调函数由业务层实现，然后通过对应的set方法传入设置 */
    F_Notifier m_notifierSecurity;    // 用户安全验证通过后，执行业务层回调函数
    F_Notifier m_notifierDisconnect;  // 用户连接断开后，执行业务层回调函数
    F_NotifierCommand m_notifierCommand;
    std::atomic_bool m_IsUpdating;

    // std::vector<yy::net::TcpConnectionPtr>  m_ShutdownConnections;
    // std::mutex                              m_ShutdownConnectionsMutex;

    std::mutex m_users_mutex;
    std::map<std::string , UserBaseDataPtr> m_users;
    std::vector<std::string> m_closeUsers;
};

}

#endif //GAMESERVER_GAMESERVER_H
