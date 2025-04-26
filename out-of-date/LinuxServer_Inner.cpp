#include <optional>
#include "LinuxServer.h"
#include "util_functions.h"
#include "log.h"
#include "md5/md5.h"
#include "optimization.h"
#include "ErrnoSaver.h"
#include "connection.pb.h"
#include "AppXmlConfig.h"

//! 本文件用以定义LinuxServer的protected/private成员函数，故取名为Inner

using namespace yy::util;


namespace yy::core {

FullDuplexPipe LinuxServer::m_sigpipe;

LinuxServer::LinuxServer():
        IServer(),
        m_app_configvar{config::g_app_config},
        m_isRunning{false},
        m_numConnect{0},
        m_numSecurity{0},
        m_notifierConnect{nullptr},
        m_notifierSecurity{nullptr},
        m_notifierDisconnect{nullptr},
        m_notifierCommand{nullptr},
        m_listenSocket{util::Socket::kInvalidFD}, m_epollList(false)
#if USE_USER_POOL
// , m_online_user_manager(m_app_configvar->getValue().app_connection_max())
#else
// , m_online_users( m_app_configvar->getValue().app_connection_max() )
#endif
{
}

void LinuxServer::LoadConfigs() // NOLINT(readability-convert-member-functions-to-static)
{
    yy::config::ConfigManager::LoadConfigs();
}

void LinuxServer::StartLog() // NOLINT(readability-convert-member-functions-to-static)
{
    yy::Ylog::LoggerManager::getInstance().ReadConfigs();
#ifdef ____DEBUG
    yy::Ylog::LoggerManager::getInstance().getLogger()->setLevel(yy::Ylog::LogLevel::eTRACE);
#else
    yy::Ylog::LoggerManager::getInstance().getLogger()->setLevel(yy::Ylog::LogLevel::eINFO);
#endif
    yy::Ylog::LogAppender::ptr appender{new yy::Ylog::StdoutLogApeender{"[%t][%l]%c%n"}};
    yy::Ylog::LoggerManager::getInstance().getLogger()->addAppender(appender);
}


void LinuxServer::SetSignals()
{
    // 屏蔽SIGPIPE：当服务器进程向已收到RST的用户套接字执行写操作时，内核会向进程发送SIGPIPE信号来结束进程
    set_signal_ignore(SIGPIPE);

    // 设置三个信号处理函数：该处理函数将信号通过管道传送给Thread_Manager
    set_signal_handler(SIGALRM, SignalHandler);
    set_signal_handler(SIGINT , SignalHandler);/* Ctrl+c */
    set_signal_handler(SIGTERM, SignalHandler);// kill <pid>

    // 注册管道的读事件，使得Thread_Manager可以读取发生的信号
    m_epollList.add_fd(LinuxServer::m_sigpipe.sideR(), EPOLLIN);
}

void LinuxServer::SignalHandler(int sig)
{
    ErrnoSaver errnoSaver;

    // 发送给管道读端，让Thread_Manager接收处理
    int msg = sig;
    LinuxServer::m_sigpipe.sideW().Write((char *)&msg, 1);
}

void LinuxServer::EnlargeHashContainer()
{
#if USE_USER_POOL
    m_online_user_manager.Init(m_app_configvar->getValue().app_connection_max());
#else
    m_online_users.reserve(m_app_configvar->GetValue().app_connection_max());
    for(int i = 0 ; i < m_app_configvar->GetValue().app_connection_max() ; ++i)
    {
        // m_online_users自身就是一个对象池，里面既有已连接的对象，也有未连接的对象，但至少不是空指针
        m_online_users.push_back(std::make_shared<UserConnection>());
    }
#endif
}

void LinuxServer::StartListen()
{
    // 初始化监听套接字
    m_listenSocket.Init(SocketType::TCP);

    // 设置为非阻塞模式
    m_listenSocket.SetNonblocking();

    // 设置套接字选项：关闭监听套接字的发送和接受缓冲区，并设置优雅关闭和端口号重复使用
    int recv_sz = 0, send_sz = 0;
    m_listenSocket.SetOpt(SO_RCVBUF, recv_sz);
    m_listenSocket.SetOpt(SO_SNDBUF, send_sz);
    m_listenSocket.SetOpt_linger(ON);
    m_listenSocket.SetOpt_reuseaddr(ON);

    // 绑定端口并开始监听
    m_listenSocket.Bind(m_app_configvar->GetValue().app_port());
    m_listenSocket.Listen(SOMAXCONN);

    //? 若使用INADDR_ANY(不指定监听套接字的IP地址)，则返回0.0.0.0的IP地址
    auto addr = m_listenSocket.GetSockname();
    YLOG_INFO("server<%s:%d>", addr->GetIPStr().c_str(), addr->GetPort())

    // epoll监视监听套接字的读事件
    m_epollList.add_fd(m_listenSocket.get_fd(), EPOLLIN); //! 监听套接字使用水平触发模式LT

    YLOG_INFO("开启监听套接字<%d>", m_listenSocket.get_fd())
}

void LinuxServer::StartThreads()
{
    m_isRunning = true;

    m_threadAccepter.reset(new std::thread(Thread_Accepter, this));
    m_threadReceiver.reset(new std::thread(Thread_Receiver, this));
    m_threadDispatcher.reset(new std::thread(Thread_Dispatcher, this));

    m_threadAccepter->detach();
    m_threadReceiver->detach();
    m_threadDispatcher->detach();
}

void LinuxServer::Thread_Dispatcher(LinuxServer *self)
{
    assert(self);
    YLOG_TRACE("Dispatcher Thread Start!")

    while(LIKELY(self->m_isRunning)) {
        size_t numEvents = self->m_epollList.wait();
        for(size_t fd = 0 ; fd < numEvents ; fd++) {
            const EpollPollerEvent& event = self->m_epollList[fd];

            //! 有新的连接，分发给Accepter线程进行accept
            if(event.get_fd() == self->m_listenSocket)
            {
                self->m_cvAccepter.notify_one();
                YLOG_TRACE("Thread_Dispatcher: Dispatch to Accepter Thread")
            }
                //! 来自管道的信息，管理线程自己处理
            else if(event.get_fd() == LinuxServer::m_sigpipe.sideR() and
                    event.is_occured(EPOLLIN))
            {
                YLOG_TRACE("Thread_Dispatcher: handle_signal_from_pipe")
                self->handle_signal_from_pipe();
            }
                //! 读事件，分发给Receiver线程
            else if(event.is_occured(EPOLLIN))
            {
                self->m_ReceiverSocketQueue.push(event.get_fd());
                YLOG_TRACE("Thread_Dispatcher: Dispatch<%d> to Reveiver Thread", event.get_fd())
            }
                //! 其它事件(不应该出现)
            else
            {
                YLOG_WARN("something else happened in <GetFD:%d> epoll_wait()...", event.get_fd())
            }
        }
    }
    YLOG_TRACE("Manager Thread Finish!")
}

void LinuxServer::handle_signal_from_pipe()
{
    char sigs[1024];
    auto nSig = LinuxServer::m_sigpipe.sideR().Read(sigs, 1);

    for(int i = 0 ; i < nSig ; ++i) {
        switch(sigs[i]) {
            // 定时器
            case SIGALRM: {
                YLOG_WARN("收到SIGALRM信号！")
                break;
            }
                // case SIGQUIT: /* Ctrl+\ */
            case SIGINT:  /* Ctrl+C */
            case SIGTERM: // kill <pid>
                // case SIGKILL: // kill -9 <pid>
            {
                YLOG_WARN("收到%s信号！", strsignal(sigs[i]))
                Stop();
                break;
            }
            default: {
                YLOG_WARN("收到其它信号！")
                break;
            }
        }
    }
}

void LinuxServer::Thread_Accepter(LinuxServer * self)
{
    assert(self);
    YLOG_TRACE("Thread_Accepter: Start!")

    while (LIKELY(self->m_isRunning)) {
        // 等待连接
        {
            std::unique_lock lg{self->m_mutexAccepter};
            self->m_cvAccepter.wait(lg);
        }
        YLOG_TRACE("Thread_Accepter: Connection Request Arrive")

        // 接受连接
        if(LIKELY(self->m_isRunning))
            self->Event_AcceptOne();
            // 收到服务器结束信号，结束线程
        else
            break;
    }

    YLOG_TRACE("Thread_Accepter: Finish!")
}

void LinuxServer::Event_AcceptOne()
{
    // 因为套接字文件描述符可以标记一个以连接用户，所以userdata一定是空闲用户对象
    Socket sock = m_listenSocket.Accept(); // LT模式进行accept

    UserConnection::ptr userdata = getFreeUser(sock);
    userdata->Init(sock);

    // 启动ET模式，并自动设置非阻塞
    m_epollList.add_fd(sock, {EPOLLIN, EPOLLET});

    // 连接数+1
    m_numConnect++;
    auto peeraddr = sock.GetPeername();
    YLOG_DEBUG("Thread_Accepter: 用户连接Accepted<%d, %s:%d>",
               sock.get_fd(), peeraddr->GetIPStr().c_str(), peeraddr->GetPort())

    // 发送随机生成的异或码给用户，之后的通信都用该异或码进行加密
    auto gen_val = genXorCode();
    // XorBody xorBody{(uint8_t)(gen_val ^ m_app_configvar->getValue().app_xor_code())};
    yy::protocol::core::XorBody xorBody;
    xorBody.set_xor_code(gen_val ^ m_app_configvar->GetValue().app_xor_code()); //! 记得与初始异或码异或
    BuildPackage(userdata, E_PackageCommand::eXor, &xorBody);
    userdata->xorCode = gen_val; //! FIXED BUG
    YLOG_TRACE("Thread_Accepter: 封包异或码<%d,%d>给用户<%d>", gen_val,xorBody.xor_code(), sock.get_fd())

    // 执行业务层的函数
    if (LIKELY(m_notifierConnect))
        m_notifierConnect(userdata, 0);
}

void LinuxServer::Thread_Receiver(LinuxServer * self)
{
    assert(self);
    YLOG_TRACE("Thread_Receiver: Start!")

    int sockfd{Socket::kInvalidFD};
    while (LIKELY(self->m_isRunning)) {
        // 等待数据到来
        self->m_ReceiverSocketQueue.wait_pop(sockfd);
        YLOG_TRACE("Thread_Receiver: Receive from <%d>", sockfd)

        // 接收sock的数据
        if(LIKELY(self->m_isRunning))
            self->Event_ReceiveOne(self->FindUserBySockfd(sockfd));
            // 收到服务器结束信号，结束线程
        else
            break;
    }

    YLOG_TRACE("Receiver Thread Finish!")
}

void LinuxServer::Event_ReceiveOne(const UserConnection::ptr& userdata) // NOLINT
{
    return_if(UNLIKELY(userdata == nullptr));

    UserBuffer & recvBuf = userdata->recv_buf;

#ifndef USE_RINGBUFFER
    // 重置recv_buf的头尾指针
    if (recvBuf.IsEmpty()) {
        recvBuf.Reset();
    }
#endif

    while(LIKELY(m_isRunning)) {
        //! ET读取数据：只会触发一次，因此在触发一次后，需要一直读取数据，直到recv返回EAGAIN(因此需要非阻塞IO)
        //! 需要使用临时缓冲区来接收数据，在数据接收完整后才拷贝到用户缓冲区，保证数据的完整性和有效性
        auto nBytesRecv = userdata->sock.Recv(
                userdata->temp_recvBuf.get(), m_app_configvar->GetValue().recv_bytes_one(), 0);

        YLOG_TRACE("Thread_Receiver: 读取<%zd>", nBytesRecv)

        // 数据读取完毕
        if (nBytesRecv < 0) {
            //! EAGAIN or EWOULDBLOCK: Recv Done
            if(nBytesRecv == -1) {
                break;
            }
                //! ECONNRESET: Connection reset by peer
            else if(nBytesRecv == -2)
            {
                YLOG_INFO("Thread_Receiver: 用户连接Reset，关闭用户连接<%d>", userdata->sock.get_fd())
                ShutdownConnection(userdata);
                break;
            }
            //! EBADF: Bad file descriptor
            else if(nBytesRecv == -3)
            {
                return;
            }
        }
            // 连接关闭请求
        else if (nBytesRecv == 0) {
            YLOG_INFO("Thread_Receiver: 用户请求关闭，关闭用户连接<%d>", userdata->sock.get_fd())
            ShutdownConnection(userdata);
            return; //! FIXED_BUG 不要漏了，因为断开连接会回收userdata，Reset之，如果再循环一次会导致bad fd错误
        }
            // 不断读取数据
        else {
            // 将数据拷贝到用户数据缓冲区中
            bool isOk = recvBuf.ReadFromCBuffer(userdata->temp_recvBuf.get(), nBytesRecv);

            // 用户发送的数据大于能接收缓冲区的最大值，连接异常，关闭之
            if(!isOk) {
                YLOG_WARN("Thread_Receiver: 用户连接发送数据过多<%zu+%zu>%zu>，终止用户连接<%d>",
                          recvBuf.DataSize(), (size_t)nBytesRecv, recvBuf.Maxsize(), userdata->sock.get_fd())
                ShutdownConnection(userdata);
            }


            // 若本次接收的数据小于一次最多能接收的数据，说明本次接收是本批recv()的最后一份数据，可以直接结束本次recv()
            if (static_cast<size_t>(nBytesRecv) < m_app_configvar->GetValue().recv_bytes_one())
                break;
        }
    }

    YLOG_TRACE("Thread_Receiver<%d>: 数据接收完毕 head-tail==%zu-%zu",
               userdata->sock.get_fd(), recvBuf.get_head(), recvBuf.get_tail())

    recvBuf.set_isCompleted(true); // Receiver线程标记数据接收完成，Handler可处理
}


// 调用Update_CheckDisconnetion()时，user不会处于Free状态
void LinuxServer::Update_CheckDisconnetion(const UserConnection::ptr& userdata) // NOLINT
{
    assert(userdata != nullptr);
    return_if(UNLIKELY(!userdata->isConnected()));

    time_t elapsed_time;

    //! 被Shutdown的用户连接在1秒后正式关闭回收资源
    elapsed_time = time(nullptr) - userdata->time_shutdown;
    if(userdata->isShutdown())
    {
        if(userdata->recv_buf.get_isCompleted() and userdata->send_buf.get_isCompleted()) {
            YLOG_INFO("<%d>主线程Update_CheckDisconnetion: 正式关闭用户连接，回收套接字资源！", userdata->sock.get_fd())
            CloseConnection(userdata);
        } else
        if(elapsed_time > 1) {
            YLOG_INFO("<%d>主线程Update_CheckDisconnetion: 时辰已到，正式关闭用户连接，回收套接字资源！", userdata->sock.get_fd())
            CloseConnection(userdata);
        }
    }

    //! ①检查已连接的用户是否在指定时间内通过安全验证，若未通过，则shutdown连接
    elapsed_time = time(nullptr) - userdata->time_connect;
    if (userdata->state == E_ServerSocketState::eConnected
        and elapsed_time > m_app_configvar->GetValue().time_security_max())
    {
        YLOG_WARN("<%d>主线程Update_CheckDisconnetion: 用户安全验证超时，关闭用户连接！", userdata->sock.get_fd())
        ShutdownConnection(userdata);
        return;
    }

    //! ②检查是否收到心跳包，如未收到，则shutdown连接
    elapsed_time = time(nullptr) - userdata->time_heart;
    if (elapsed_time > m_app_configvar->GetValue().time_heart_max()) {
        YLOG_WARN("<%d>主线程Update_CheckDisconnetion: 用户心跳包超时，关闭用户连接！", userdata->sock.get_fd())
        ShutdownConnection(userdata);
        return;
    }
}

//! recv_buf的消费者：在Update中调用，解析包，并上交至业务层执行
/* 数据已被接受到buff中，消费缓冲区的数据(消费者)，解析消息头，根据消息头中的指令cmd字段选择解析消息体的函数执行 */
void LinuxServer::Update_ReadPackage(const UserConnection::ptr& userdata) // NOLINT
{
    assert(userdata != nullptr);
    return_if(UNLIKELY(!userdata->isGood())); //? 是否要继续处理被shutdown的用户的接收缓冲区？

    UserBuffer& recvBuf = userdata->recv_buf;
    //! 循环的作用：如果在刚好处理完后，缓冲区又被填充，如果要再等下一次Update的话就太慢了！
    //! 因此这里进行有限次循环，可以处理来自用户的一串包
    //! （但循环次数不能太多，也不能无限循环，否则服务器可能会一直接收来自userdata的数据，而使得其他用户忙等）
    // auto t1 = Ticker<>::tick();
    for(int i = 0; i < 1000 ;++i)
    {
        // 数据还没准备好，跳过本次Update
        if(!recvBuf.get_isCompleted()) {
            YLOG_TRACE("<%d>主线程Update_Read: <head-tail>==<%zu-%zu>，数据还没准备好，跳过本次Update_ReadPackage",
                       userdata->sock.get_fd(), recvBuf.get_head(), recvBuf.get_tail())
            return;
        }

        /* 解析消息头：如果消息头有效，则移动buff.head */
        auto && pkg_head = ProcessHead(userdata);

        YLOG_TRACE("<%d>主线程Update_Read: <head-tail>==<%zu-%zu>, 读取消息头",
                   userdata->sock.get_fd(), recvBuf.get_head(), recvBuf.get_tail())

        // 若没有消息体，则可以直接跳出循环
        if(!pkg_head.has_value())
            break;

        /* 解析消息体：由业务层的ParsePackage移动buff.head */
        userdata->package_len = pkg_head->length;
        //! 根据消息指令执行对应函数，该函数会进入业务层并从业务层返回
        ProcessCommand(userdata, pkg_head.value());

        // *return： 命令执行完毕后，若用户断开连接
        if(!userdata->isConnected()) {
            YLOG_INFO("<%d>主线程Update_Read: 客户由命令关闭连接....\n", userdata->sock.get_fd())
            return;
        }
    }
    // auto t2 = Ticker<>::tick();
    // if(t2-t1 > 0.01)
    //     YLOG_INFO("%lf", t2-t1);

    // 数据已被接收在buf中(isCompleted=true)，但消息未接收完整 or 消息消费完毕(isCompleted=false)
    recvBuf.set_isCompleted(false);
}

std::optional<PackageHead> LinuxServer::ProcessHead(const UserConnection::ptr& userdata) // NOLINT
{
    UserBuffer& recvBuf = userdata->recv_buf;

    PackageHead pkg_head;
    bool isOk = pkg_head.ReadFromBuffer(recvBuf, userdata->xorCode);
    if(!isOk) {
        YLOG_TRACE("<%d>主线程Update_Read: <head-tail>==<%zu-%zu>，消息(头)还未接收完全，跳过本次Update_ReadPackage",
                   userdata->sock.get_fd(), recvBuf.get_head(), recvBuf.get_tail())
        return std::nullopt;
    }

    // // *return：消息头验证：若验证失败，表明不是本协议的数据包，关闭连接
    if(strncmp(pkg_head.check_code, m_app_configvar->GetValue().check_code(), sizeof(pkg_head.check_code)) != 0)
    {
        YLOG_WARN("<%d>解包: 消息头<%d, %d>验证失败！关闭用户连接",
                  userdata->sock.get_fd(), pkg_head.check_code[0], pkg_head.check_code[1])
        ShutdownConnection(userdata);
        return std::nullopt;
    }

    YLOG_TRACE("<%d>解包：其首部信息为：%c%c-%d-%d", userdata->sock.get_fd(),
               pkg_head.check_code[0], pkg_head.check_code[1], pkg_head.length, pkg_head.cmd)

    // *return： 消息体还未接收完全，跳过本次Update（注意不要在此时移动head！要保证整个包的完整性）
    if(recvBuf.DataSize() < pkg_head.CalcBodyLen()) {
        YLOG_DEBUG("<%d>解包: 消息体还未接收完全<%zu>，跳过本次Update_ReadPackage", userdata->sock.get_fd(), recvBuf.DataSize())
        recvBuf.BackHead(sizeof(pkg_head)); // 指针回退
        return std::nullopt;
    }

    return pkg_head;
}

void LinuxServer::ProcessCommand(const UserConnection::ptr& userdata, const PackageHead& pkg_head) // NOLINT
{
    assert(userdata != nullptr);
    // return_if(UNLIKELY(!userdata->isConnected())); // 不能返回，因为数据已读入用户缓冲，shutdown情况下需要处理这些数据

    userdata->time_heart = time(nullptr);

    switch((E_PackageCommand)pkg_head.cmd)
    {
        // 收到心跳包
        case E_PackageCommand::eHeart:
        YLOG_DEBUG("<%d>解包：收到心跳包！", userdata->sock.get_fd())
            OnHeart(userdata);
            break;
            // 收到安全验证请求
        case E_PackageCommand::eSecurity:
        YLOG_DEBUG("<%d>解包：收到用户的安全验证请求，进行安全验证！", userdata->sock.get_fd())
            OnSecurity(userdata);
            break;
        default:
            if(m_notifierCommand)
                m_notifierCommand(userdata, (int32_t)pkg_head.cmd);
            break;
    }
}

void LinuxServer::OnHeart(const UserConnection::ptr& userdata)
{
    assert(userdata != nullptr);

    // 只需发一个只有消息头的包
    BuildPackage(userdata, E_PackageCommand::eHeart, nullptr);
}

void LinuxServer::OnSecurity(const UserConnection::ptr& userdata) // NOLINT
{
    assert(userdata != nullptr);

    char md5Arr[35]{};
    char Arr[30]{};

    snprintf(Arr, sizeof(Arr), "%s_%d", m_app_configvar->GetValue().security_code(), userdata->xorCode);
    md5::EncryptMD5str(md5Arr, (unsigned char *)(Arr), (int)strlen(Arr));

    // 读取安全验证请求
    YLOG_TRACE("<%d>解包执行：读取用户安全认证信息至结构体中", userdata->sock.get_fd())
    yy::protocol::core::SecurityBody securityBody;
    ParsePackage(userdata, &securityBody);

    YLOG_DEBUG("服务器: %d, %d, %s", m_app_configvar->GetValue().app_id(), m_app_configvar->GetValue().app_version(), md5Arr)
    YLOG_DEBUG("客户端: %d, %d, %s", securityBody.app_id(),securityBody.app_version(), securityBody.app_md5().c_str())

    // 进行安全验证，并返回验证结果给用户
    yy::protocol::core::ResultCode resultCode;
    if(securityBody.app_version() != m_app_configvar->GetValue().app_version()) {
        YLOG_DEBUG("<%d>解包执行：版本不同，安全验证失败！", userdata->sock.get_fd())
        resultCode = yy::protocol::core::ResultCode::eAppVersionFailed;
    }
    else if(util::StrCmp_IgnoreCase(securityBody.app_md5().c_str(), md5Arr)) {
        YLOG_DEBUG("<%d>解包执行：md5码不同，安全验证失败！", userdata->sock.get_fd())
        resultCode = yy::protocol::core::ResultCode::eMd5Failed;
    }
    else {
        resultCode = yy::protocol::core::ResultCode::eSuccess;
    }
    yy::protocol::core::ResultBody resultBody;
    resultBody.set_result_code(resultCode);
    BuildPackage(userdata, E_PackageCommand::eSecurity, &resultBody);

    // 安全验证通过：交由业务层
    if(resultBody.result_code() == yy::protocol::core::ResultCode::eSuccess) {
        userdata->appID = securityBody.app_id();
        userdata->state = E_ServerSocketState::eSecure; // 转换状态
        m_numSecurity++;
        if(m_notifierSecurity)
            m_notifierSecurity(userdata, (int32_t)resultBody.result_code());
        YLOG_INFO("<%d>解包执行：安全验证通过", userdata->sock.get_fd())
    }
        //? 安全验证失败：需要关闭用户连接吗？
    else {
        YLOG_INFO("<%d>解包执行：用户安全验证失败！", userdata->sock.get_fd())
    }
}

//! send_buf的消费者：在Update中调用，发送用户数据
void LinuxServer::Update_SendPackage(const UserConnection::ptr& userdata) // NOLINT
{
    assert(userdata != nullptr);
    // *return：用户连接会在Receiver线程Reset，不能保证userdata内容的有效性，这时便需要返回
    return_if(UNLIKELY(!userdata->isGood()));  //! 被Shutdown的用户连接不能被send

    UserBuffer& sendBuf = userdata->send_buf;

    auto nBytesSend = sendBuf.WriteToSocket(userdata->sock);
    if(nBytesSend > 0) {
        YLOG_TRACE("<%d>主线程Update_Send：长%zdB数据包发送给用户, head-tail==%zu-%zu",
                   userdata->sock.get_fd(), nBytesSend, sendBuf.get_head(), sendBuf.get_tail())
    }
    else
    if(nBytesSend == 0) {
        YLOG_TRACE("<%d>主线程Update_Send：没有数据发送，跳过Send, head-tail==%zu-%zu",
                   userdata->sock.get_fd(), sendBuf.get_head(), sendBuf.get_tail())
    }
    else {
        YLOG_ERROR("<%d>主线程Update_Send：send()返回0，可能是用户连接已关闭, head-tail==%zu-%zu",
                   userdata->sock.get_fd(), userdata->send_buf.get_head(), userdata->send_buf.get_tail())
        ShutdownConnection(userdata);
    }
}

//! 该函数不一定在主线程中运行，也会在Event_ReceiveOne()用户连接断开时调用，此时userdata被Reset
//! 因此userdata内的数据并不是一直有效的，所以在用到userdata的地方都需要判断其有效性，即userdata->state是否已连接

LinuxServer::~LinuxServer()
{
    Stop();
}

void LinuxServer::ShutdownConnection(const UserConnection::ptr& userdata) { // NOLINT
    assert(userdata != nullptr);
    return_if(!userdata->isGood());

    YLOG_DEBUG("<%d>: Shutdown用户连接", userdata->sock.get_fd())

    // 设置缓冲区
    userdata->recv_buf.set_isCompleted(true);
    userdata->send_buf.set_isCompleted(true);
    userdata->is_shutdown = true;
    userdata->sock.Shutdown();
}

void LinuxServer::CloseConnection(const UserConnection::ptr& userdata) { // NOLINT
    assert(userdata != nullptr);
    return_if(!userdata->isConnected());

    m_epollList.del_fd(userdata->sock.get_fd());  // 取消监视
    userdata->sock.Close();                           // 关闭套接字，同时该文件描述符及其资源将被回收
    m_numConnect--;                                   // 连接数-1
    if(userdata->isSecure())
        m_numSecurity--;                              // 安全连接数-1

    // 连接关闭了，连接数据已清除，但是玩家游戏数据仍保存在业务层，调用业务层的函数，由业务层保存玩家数据
    if (m_notifierDisconnect)
        m_notifierDisconnect(userdata, 0);

}

void LinuxServer::setUserFree(const UserConnection::ptr& userdata)
{
#if USE_USER_POOL
    m_online_user_manager.delUser(userdata->sock);
#else
    userdata->Reset();
#endif
}

UserConnection::ptr LinuxServer::getFreeUser(Socket sock)
{
#if USE_USER_POOL //todo
    m_online_user_manager.addUser(sock);
    return m_online_user_manager.findUserBySockfd(sock.get_fd());
#else
    return m_online_users[sock.get_fd()];
#endif
}




} // namespace yy::server

