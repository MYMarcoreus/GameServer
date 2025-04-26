#ifndef ____LINUX_SERVER_H
#define ____LINUX_SERVER_H

#include "IServer.h"
#include "UserConnection.h"
#include "Socket.h"
#include "ConfigManager.h"
#include "ThreadSafeQueue.hpp"
#include "ObjectPool.h"
#include "OnlineUserManager.h"
#include "AppXmlConfig.h"
#include <unordered_map>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <optional>

// USE_USER_POOL不好，有些小bug
#define USE_USER_POOL false


namespace yy::core {

class LinuxServer final : public IServer, public Singleton<LinuxServer>
{
    SINGLETON_NECESSITY(LinuxServer)
    friend class OnlineUserManager;
public:
    void Start() override;

    void Stop() override;

    void Start() override;

    /// @brief 根据指令cmd，将指定类型「序列化」为send_buf中的字节流，并且添上首部
    void BuildPackage(
            const UserConnection::ptr& userdata, E_PackageCommand cmd,
            const google::protobuf::Message * body) override; //! send_buf的生产者

    /// @brief recv_buf已被Thread_Receiver填充，将recv_buf中的字节流「结构化」为指定类型
    // (在Update_ReadPackage()->ProcessCommand()中调用，不改变head或tail)
    void ParsePackage(const UserConnection::ptr& userdata, google::protobuf::Message * data) override;

    UserConnection::ptr FindUserBySockfd(int sockfd) override;

    /*** GETTER & SETTER ***/
    [[nodiscard]] size_t getConnnectionCount()       const override { return m_numConnect; }  // 连接数
    [[nodiscard]] size_t getSecureConnnectionCount() const override { return m_numSecurity; } // 安全连接数
    [[nodiscard]] bool   IsRunning() const override { return m_isRunning; };

    void setNotifier_Connect   (F_Notifier f) override { m_notifierConnect = f; }
    void SetNotifier_Security  (F_Notifier f) override { m_notifierSecurity = f; }
    void SetNotifier_DisConnect(F_Notifier f) override { m_notifierDisconnect = f; }
    void setNotifier_Command   (F_Notifier f) override { m_notifierCommand = f; }

    UserConnection::ptr getFreeUser(yy::net::Socket & sock) override;
    void setUserFree(const UserConnection::ptr& userdata) override;

    config::ConfigVar<config::AppXmlConfig>::ptr GetAppConfig() override { return m_app_configvar; }

private:
    LinuxServer();
    ~LinuxServer() override;


/* Start()函数所调用 */
    void LoadConfigs();

    void StartLog();

    void EnlargeHashContainer();

    void StartListen();

    void SetSignals();
    static void SignalHandler(int sig);

    void StartThreads();


/* 线程管理：三个线程 */
    static void Thread_Dispatcher(LinuxServer* self);
    void handle_signal_from_pipe();

    static void Thread_Accepter(LinuxServer* self);
    void Event_AcceptOne();

    static void Thread_Receiver(LinuxServer* self); //! recv_buf的生产者
    void Event_ReceiveOne(const UserConnection::ptr& userdata);


/* Update */
    /// @brief 在Update中调用，解析包
    void Update_ReadPackage(const UserConnection::ptr& userdata); //! recv_buf的消费者
    std::optional<PackageHead> ProcessHead(const UserConnection::ptr& userdata);
    void ProcessCommand(const UserConnection::ptr& userdata, const PackageHead& pkg_head);
    void OnHeart(const UserConnection::ptr& userdata);
    void OnSecurity(const UserConnection::ptr& userdata);

    /// @brief 在Update中调用，发送用户数据
    void Update_SendPackage(const UserConnection::ptr& userdata); //! send_buf的消费者


/* 连接管理 */
    /// @brief 关闭用户连接，但是不回收文件描述符，仍保留系统分配的套接字的资源(如缓存)，适合用户掉线可能马上再连接的情况
    void ShutdownConnection(const UserConnection::ptr& userdata);
    /// @brief 在Update中调用，
    ///     ①检查正在连接的用户是否在指定时间内通过安全验证
    ///     ②检查是否收到心跳包
    /// 若以上两点有一点未完成，则关闭连接
    void Update_CheckDisconnetion(const UserConnection::ptr& userdata);
    /// @brief 关闭用户连接，而且回收文件描述符，释放用户套接字的资源，适合用户连接确认已经关闭的情况
    /// Shutdown之后，等待在Update_CheckDisconnetion()中调用CloseConnection()以回收套接字文件描述符
    void CloseConnection(const UserConnection::ptr& userdata);


private:
    config::ConfigVar<config::AppXmlConfig>::ptr m_app_configvar; // 用于获取配置项

    /* 在Linux下可以用套接字文件描述符来标记一个已连接用户，并可以在关闭连接时使用close(不能用shutdown)来回收该文件描述符 */
#if USE_USER_POOL
    // 使用对象池时：索引只反映了连接的时间先后，因此需要使用fd2index来保存套接字文件描述符到索引的映射
    OnlineUserManager m_online_user_manager;
#else
    // 不使用对象池时：用套接字文件描述符(int)来做索引
    std::vector<UserConnection::ptr> m_online_users;
#endif

    std::atomic<size_t> m_numConnect;  //当前连接数
    std::atomic<size_t> m_numSecurity; //安全连接数

    /* 这几个回调函数由业务层实现，然后通过对应的set方法传入设置 */
    F_Notifier m_notifierConnect;     // 用户连接成功后，执行业务层回调函数
    F_Notifier m_notifierSecurity;    // 用户安全验证通过后，执行业务层回调函数
    F_Notifier m_notifierDisconnect;  // 用户连接断开后，执行业务层回调函数
    F_Notifier m_notifierCommand;     // 读取用户数据包时，若分析到指令是业务层指令，则执行业务层回调函数

    /* 
     *  「事件分发线程」进行epoll_wait()的线程，监视套接字的accept()和“读数据”这两个事件，
     *  事件发生时将其分发给「连接接收线程」和「数据接收线程」
     *  （这两个线程使用等待条件变量，由「事件分发线程」唤醒之）
     *  而主线程负责不停Update()(在Update里面发送数据和解析数据(注意不是接收)：也就是说「主线程是数据发送、解析线程」)
     */
    // 事件分发线程
    std::shared_ptr<std::thread> m_threadDispatcher;
    // 连接接受线程
    std::shared_ptr<std::thread> m_threadAccepter;
    std::condition_variable      m_cvAccepter;
    std::mutex                   m_mutexAccepter;
    // 数据接收线程
    std::shared_ptr<std::thread> m_threadReceiver;
    util::ThreadSafeQueue<int>   m_ReceiverSocketQueue; // 用于Manager和Receiver同步

    // 控制线程是否运行，由信号或主线程Stop时设置
    std::atomic<bool> m_isRunning;

    // 两端都可读写的管道，用于传输信号
    static util::FullDuplexPipe m_sigpipe;

    // 套接字和epoll
    util::Socket              m_listenSocket; // 监听套接字
    util::EpollList           m_epollList;    // epoll监视事件列表
};






} // namespace yy







#endif // ____LINUX_SERVER_H


