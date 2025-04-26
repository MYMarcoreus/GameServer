#ifndef ____ISERVER_H
#define ____ISERVER_H

#include "core_definations.h"
#include "noncopyable.h"
#include "ConfigManager.h"
#include "AppXmlConfig.h"
#include "net_definations.h"




namespace yy::core {


//! 因为F_Notifier需要传入服务器的this指针，不要使用智能指针去接收this指针，这也许会带来许多麻烦/bug
//! 因此尽量使用引用接收*this

class IServer: util::noncopyable
{
public:
    using ptr = std::shared_ptr<IServer>;
    using F_Notifier = std::function<void(const yy::net::TcpConnectionPtr &)>;
    using F_NotifierCommand = std::function<void(const UserConnectionPtr &, const MessagePtr &)>;
public:
    IServer() = default;
 
    virtual ~IServer() = default;

    /// @brief 开始监听并启动IO线程
    virtual void Start() = 0;

    /// @brief 结束服务器
    virtual void Stop() = 0;

    virtual UserConnectionPtr FindUser(const std::string & conn_name) = 0;
    virtual void            DelUser(const std::string & conn_name) = 0;
    virtual void            AddUser (const std::string & conn_name, const UserConnectionPtr &) = 0;

    virtual bool IsRunning() const = 0;

    virtual const config::AppXmlConfig & GetAppConfig() = 0;


    /* 在实现类中定义四个回调函数成员，下面这四个函数将会设置其对应的回调函数，而回调函数将由业务层定义并传入 */
    virtual void SetNotifier_Security  (F_Notifier e) = 0;
    virtual void SetNotifier_DisConnect(F_Notifier e) = 0;
    virtual void SetNotifier_Command(F_NotifierCommand cb) = 0;

    virtual net::TimerID RunAt(net::Timestamp time, net::F_TaskCallback cb) = 0;
    virtual net::TimerID RunAfter(net::Microseconds delay, net::F_TaskCallback cb) = 0;
    virtual net::TimerID RunEvery(net::Microseconds interval, net::F_TaskCallback cb) = 0;
    virtual void CancelTimer(net::TimerID timerid) = 0;
};

// 用于实现跨平台的函数：在此返回LinuxServer的实例
// extern IServer * get_server_instance();




} // namespace yy








#endif // !____ISERVER_H
