#pragma once

#include "net_definations.h"
#include "core_definations.h"
#include "noncopyable.h"
#include "AppXmlConfig.h"

using namespace yy::net;

namespace yy::core {

class IServer: util::noncopyable
{
public:
    using ptr = std::shared_ptr<IServer>;
    using F_Notifier = std::function<void(const UserConnectionPtr &)>;
    using F_NotifierCommand = std::function<void(const UserConnectionPtr &, const MessagePtr &, MessageNetType)>;
public:
    IServer() = default;
 
    virtual ~IServer() = default;

    /// @brief 开始监听并启动IO线程
    virtual void Start(const F_ThreadInitCallback& cb = nullptr) = 0;

    /// @brief 结束服务器
    virtual void Stop() = 0;

    virtual UserConnectionPtr FindUser(uint64_t conn_id) = 0;
    virtual void              DelUser(uint64_t conn_id) = 0;
    virtual void              AddUser (uint64_t conn_id, const UserConnectionPtr &) = 0;

    virtual bool IsRunning() const = 0;

    virtual const config::AppXmlConfig & GetAppConfig() = 0;

    virtual IPAddressPtr GetListenAddr() const = 0;

    /* 在实现类中定义四个回调函数成员，下面这四个函数将会设置其对应的回调函数，而回调函数将由业务层定义并传入 */
    virtual void SetNotifier_Security  (F_Notifier e) = 0;
    virtual void SetNotifier_DisConnect(F_Notifier e) = 0;
    virtual void SetNotifier_Command(F_NotifierCommand cb) = 0;

    virtual TimerID RunAt(Timestamp time, F_TaskCallback cb) = 0;
    virtual TimerID RunAfter(Microseconds delay, F_TaskCallback cb) = 0;
    virtual TimerID RunEvery(Microseconds interval, F_TaskCallback cb) = 0;
    virtual void CancelTimer(TimerID timerid) = 0;
};

// 用于实现跨平台的函数：在此返回LinuxServer的实例


} // namespace yy


