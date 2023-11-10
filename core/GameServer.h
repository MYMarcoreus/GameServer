#ifndef GAMESERVER_GAMESERVER_H
#define GAMESERVER_GAMESERVER_H

#include "IServer.h"
#include "Singleton.h"
#include "TcpServer.h"
#include "codec/ProtobufCodec.h"
#include "codec/ProtobufDispatcher.h"
#include "net_definations.h"
#include "ThreadPool.h"


namespace yy::core {


namespace protocol {
class HeartBody;
class SecurityBody;
}



class GameServer: public IServer{

    using HeartPtr    = std::shared_ptr<yy::core::protocol::HeartBody> ;
    using SecurityPtr = std::shared_ptr<yy::core::protocol::SecurityBody> ;

public:
    GameServer(yy::net::EventLoop* loop, yy::net::IPAddressPtr listenAddr);
    ~GameServer() override;

    /// @brief 启动并初始化服务器
    virtual void Start() override {
        m_server.Start(1);
        m_workThreads.Start(1);
    }

    /// @brief 结束服务器
    virtual void Stop() override {
        m_workThreads.Stop();
    }

    /// @brief 在业务层的while(1)中调用Update
    virtual void Update() override {}

    /// @brief 通过套接字文件描述符寻找用户连接数据
    virtual UserBaseDataPtr & FindUser(const yy::net::TcpConnectionPtr conn) override {}
    virtual bool   isRunning() const override {}


    virtual const config::AppXmlConfig & GetAppConfig() override { return m_app_configvar->GetValue(); }


    /* 在实现类中定义四个回调函数成员，下面这四个函数将会设置其对应的回调函数，而回调函数将由业务层定义并传入 */
    virtual void setNotifier_Security  (F_Notifier cb) override { m_notifierSecurity   = cb; }
    virtual void setNotifier_DisConnect(F_Notifier cb) override { m_notifierDisconnect = cb; }

    template<typename T>
    void RegisterMessageCallback(const CallbackT<T>::ProtobufMessageTCallback &callback) {
        m_dispatcher.RegisterMessageCallback<T>(callback);
    }



private:
    void OnUnknownMessage(yy::net::TcpConnectionPtr conn, const MessagePtr& message);

    void OnConnectionEstablished(yy::net::TcpConnectionPtr conn);
    void SendXorCode(const yy::net::TcpConnectionPtr &conn);

    void OnHeart(const yy::net::TcpConnectionPtr & conn, const HeartPtr & message);
    void OnSecurity(const yy::net::TcpConnectionPtr & conn, const SecurityPtr & message);


    void AddShutdownConnection(const yy::net::TcpConnectionPtr &conn);
    void CheckDisconnections();

private:
    yy::net::EventLoop *        m_loop;
    yy::net::TcpServer          m_server;
    ProtobufCodec               m_codec;
    ProtobufDispatcher          m_dispatcher;

    yy::config::ConfigVar<yy::config::AppXmlConfig>::ptr    m_app_configvar; // 用于获取配置项
    std::atomic<size_t> m_NumSecurity; //安全连接数

    /* 这几个回调函数由业务层实现，然后通过对应的set方法传入设置 */
    F_Notifier m_notifierSecurity;    // 用户安全验证通过后，执行业务层回调函数
    F_Notifier m_notifierDisconnect;  // 用户连接断开后，执行业务层回调函数

    std::vector<yy::net::TcpConnectionPtr>  m_ShutdownConnections;
    std::mutex                              m_ShutdownConnectionsMutex;

    std::map<yy::net::TcpConnection *, UserBaseDataPtr> m_users;


    yy::util::ThreadPool m_workThreads;
};

}

#endif //GAMESERVER_GAMESERVER_H
