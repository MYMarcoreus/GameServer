#ifndef ____ISERVER_H
#define ____ISERVER_H

#include <google/protobuf/message.h>
#include "UserBaseData.h"
#include "ConfigManager.h"
#include "AppXmlConfig.h"


namespace yy::core {

//! 因为F_Notifier需要传入服务器的this指针，不要使用智能指针去接收this指针，这也许会带来许多麻烦/bug
//! 因此尽量使用引用接收*this

class IServer: util::noncopyable
{
public:
    using ptr = std::shared_ptr<IServer>;
    // 第三个参数主要是为了留给传输消息头指令使用的
    using F_Notifier = void (*)(const UserBaseData::ptr &, int32_t);
public:
    IServer() = default;

    virtual ~IServer() = default;

    /// @brief 启动并初始化服务器
    virtual void Start() = 0;

    /// @brief 结束服务器
    virtual void Stop() = 0;

    /// @brief 在业务层的while(1)中调用Update，不断地解包、封包
    virtual void Update() = 0;

    /// @brief 封包(结构体-->字节流(用户发送缓冲))：业务层调用，将结构体转化为字节流写到用户的发送缓冲中
    virtual void BuildPackage(
            const UserBaseData::ptr& userdata,
            E_PackageCommand cmd,
            const google::protobuf::Message * data ) = 0;

    /// @brief 解包(结构体<--字节流(用户接收缓冲)：业务层调用，将用户接收缓冲区的消息体字节流读取到业务层结构体中
    virtual void ParsePackage(const UserBaseData::ptr& userdata, google::protobuf::Message * data) = 0;

    /// @brief 通过套接字文件描述符寻找用户连接数据
    virtual UserBaseData::ptr FindUserBySockfd(int sockfd) = 0;

    [[nodiscard]] virtual size_t getConnnectionCount() const = 0;        // 连接数
    [[nodiscard]] virtual size_t getSecureConnnectionCount() const = 0;  // 安全连接数
    [[nodiscard]] virtual bool isRunning() const = 0;

    virtual UserBaseData::ptr getFreeUser(util::Socket sock) = 0;
    virtual void setUserFree(const UserBaseData::ptr& userdata) = 0;

    virtual config::ConfigVar<config::AppXmlConfig>::ptr GetAppConfig() = 0;


    /* 在实现类中定义四个回调函数成员，下面这四个函数将会设置其对应的回调函数，而回调函数将由业务层定义并传入 */
    virtual void setNotifier_Connect   (F_Notifier e) = 0;
    virtual void setNotifier_Security  (F_Notifier e) = 0;
    virtual void setNotifier_DisConnect(F_Notifier e) = 0;
    virtual void setNotifier_Command   (F_Notifier e) = 0;
};

// 用于实现跨平台的函数：在此返回LinuxServer的实例
extern IServer& get_server_instance();


} // namespace yy








#endif // !____ISERVER_H