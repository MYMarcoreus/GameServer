#include "ConfigManager.h"
#include "LinuxServer.h"
#include "util_functions.h"
#include "log.h"
#include "optimization.h"
#include "connection.pb.h"

using namespace yy::util;



namespace yy::core {

IServer& get_server_instance()
{
    return LinuxServer::getInstance();
}


void LinuxServer::Start()
{

    // 1、读取配置文件：必须是第一个运行
    LoadConfigs();

    // 2、开启日志：读取日志配置
    StartLog();

    // 3、哈希表扩容，提前申请内存空间
    EnlargeHashContainer();

    // 4、设置信号处理函数(通过管道传送给Thread Manager)，屏蔽SIGPIPE，设置监视管道读信号
    SetSignals();

    // 5、开启监听套接字，并将其读事件加入epoll监视事件列表
    StartListen();

    // 6、开启三个线程
    StartThreads();
}

void LinuxServer::Stop()
{
    if(LIKELY(m_isRunning)) {
        YLOG_INFO("Server Stop!")

        m_isRunning = false;
        /* 设置完m_isRunning标志，在Stop结束后，Manager线程会随即检测该标志，
           而其他两个线程会在收到下面两个信号量之后检测该标志以结束线程 */
        m_cvAccepter.notify_all(); // 发送结束信号给Accepter线程
        m_ReceiverSocketQueue.push(Socket::kInvalidFD); // 发送结束信号给Receiver线程

        m_listenSocket.Close();
#if USE_USER_POOL
        for(int i = 0 ; i < m_online_user_manager.size() ; ++i) {
            CloseConnection(m_online_user_manager.findUserByIndex(i));
        }
#else
        // 关闭套接字
        for(auto & userdata: m_online_users)
        {
            CloseConnection(userdata);
        }
#endif
    }
}

//! send_buf的生产者：根据指令cmd，将指定类型「序列化」为send_buf中的字节流，并且添上首部
void LinuxServer::BuildPackage(
        const UserBaseData::ptr& userdata, E_PackageCommand cmd,
        const google::protobuf::Message * body)
{
    assert(userdata != nullptr);
    return_if(!userdata->isGood()); // 如果封包后要发送的用户连接已终止，就没必要再封包了

    //! 封包，操作tail(生产者)，将数据生产到send_buf中，完成后由Update_Send()在主线程中发送从[m_CheckCode, tail)的数据
    UserBuffer & sendBuf = userdata->send_buf;
    size_t head_len = sizeof(PackageHead);
    size_t body_len = (body ? body->ByteSizeLong() : 0);

    YLOG_DEBUG("封包：<%d>消息头长%zuB，消息体长%zudB", userdata->sock.get_fd(), head_len, body_len)

    //TODO 发送缓冲区的剩余空间无法容纳待封装的包（该情况可以通过环形缓冲区改进）
    if(head_len+body_len > sendBuf.RemainedSize()) {
        YLOG_WARN("<%d>发送缓冲区的剩余空间无法容纳待封装的包，干脆断开连接", userdata->sock.get_fd())
        ShutdownConnection(userdata);
        return;
    }

#ifndef USE_RINGBUFFER
    // 是新的一批数据，将send_buf初始化
    if(sendBuf.IsEmpty()) {
        sendBuf.Reset();
    }
#endif

    /* 封装消息头 */
    PackageHead pkg_head;
    pkg_head.set_check_code(m_app_configvar->GetValue().check_code(), userdata->xorCode);
    pkg_head.set_length(head_len+body_len, userdata->xorCode);
    pkg_head.set_cmd(cmd, userdata->xorCode);
    sendBuf.ReadFromStruct(pkg_head);

    YLOG_DEBUG("<%d>封包：消息头原文(D-E:%zu:%hu)", userdata->sock.get_fd(), head_len+body_len, (int)cmd)
    YLOG_DEBUG("<%d>封包：消息头密文(%d-%d:%u:%d)", userdata->sock.get_fd(),
               pkg_head.check_code[0], pkg_head.check_code[1], pkg_head.length, (int)pkg_head.cmd)

    /* 封装消息体(如果有的话，如心跳包就没有消息体) */
    if(body_len > 0) {
        YLOG_DEBUG("<%d>封包：消息体长%zuB", userdata->sock.get_fd() ,body_len)
        sendBuf.ReadFromProtobuf(body);
    }

    YLOG_TRACE("<%d>主线程Update_SendPackage: 数据待发送，head-tail==%zu-%zu",
               userdata->sock.get_fd(), sendBuf.get_head(), sendBuf.get_tail())
}

//! recv_buf的消费者：将recv_buf中的字节流「结构化」为指定类型
// 消息头已被解析完毕，数据已在recv_buf中
void LinuxServer::ParsePackage(const UserBaseData::ptr& userdata, google::protobuf::Message * data)
{
    assert(userdata != nullptr);
    return_if(data == nullptr);
    // return_if(!userdata->isGood()); // 不用返回，因为这是被shutdown用户的“遗言”，业务层可以调用该函数，以完成“遗愿”

    // Receiver线程已经将数据读取保存到recv.buf的[head, tail)中，之后包头被解析，现在recvBuf.head指向消息体的起点
    UserBuffer & recvBuf = userdata->recv_buf;
    size_t data_len = userdata->package_len - sizeof(PackageHead);

    YLOG_TRACE("ParsePackage<%d>：data_len = %zu", userdata->sock.get_fd(),data_len)

    if(data_len > 0) {
        YLOG_TRACE("ParsePackage<%d>: 数据读取完毕 head-tail==%zu-%zu",
                   userdata->sock.get_fd(), recvBuf.get_head(), recvBuf.get_tail())
        // 将缓冲的数据读取到数据结构，并移动recvBuf.head
        bool isOk = recvBuf.WriteToProtobuf(data, data_len);
        if(!isOk) {
            YLOG_TRACE("ParsePackage<%d>: 收到的数据的长度<%zu>不够解析", userdata->sock.get_fd() , data_len)
            return;
        }
        // YLOG_TRACE("<%d>已将长为<%zu>数据读取到结构体中", userdata->sock.get_fd() , data_len)
    }
}


void LinuxServer::Start()
{
    INTERVAL_DO(1 ,YLOG_DEBUG("usernum = %lu", m_numConnect.load()) )

    /// 最大的问题：userdata指向的用户数据不一定总是有效
#if USE_USER_POOL
    for(int i = 0 ; i < m_online_user_manager.size() ; ++i)
    {
        //userdata不能是引用，若取得后delUser()交换了指针，则userdata是薛定谔的猫
        //若Update_ReadPackage(userdata)时是内存A，则delUser(userdata.sock)后，
        //Update_SendPackage(userdata)中是内存B，这不对
        auto userdata = m_online_user_manager.findUserByIndex(i);
#else
    // 遍历每一个在线的用户数据
    for (const auto& userdata: m_online_users)
    {
#endif
        assert(userdata != nullptr);

        /*! 使用对象池时，使用线程安全的队列，在Accept时取出对象，在CloseConnection时归还对象
          ! 但是对象池仍不能保证m_online_users都是已连接的用户
          ! 因为在Thread_Receiver线程中的Event_ReceiveOne()，会断开用户连接，
          ! 然后调用userdata.Reset()，从而导致用户数据失效 */
        /*! 不使用对象池时，时m_online_users里面并不全是已连接用户，也包含了“对象池”
          ! 需要额外遍历那些未连接的用户对象，这也许很浪费时间 */
        // isNeedSave()成立时，说明用户数据等待被业务层保存
        continue_if(!userdata->isConnected() or userdata->isNeedSave());

        INTERVAL_DO(1, YLOG_DEBUG("user{%d, %d, %d}", userdata->sock.get_fd(), userdata->xorCode, (int)userdata->state) )

        // 检查需要断开连接的用户
        Update_CheckDisconnetion(userdata);

        //? 是否要继续处理被shutdown的用户的接收缓冲区？
        continue_if(userdata->isShutdown());

        // 解析数据：用户数据已被Receiver填充到userdata->recv_buf
        // 根据消息的类型将要发送的数据填充到userdata->send_buf
        Update_ReadPackage(userdata);

        // 发送数据：send_buf已在Update_ReadPackage()中被填充，
        // 故这里直接将userdata->send_buf发送给用户
        Update_SendPackage(userdata);
    }

}

UserBaseData::ptr LinuxServer::FindUserBySockfd(int sockfd)
{
    if(sockfd < 0)
        return nullptr;

#if USE_USER_POOL
    auto userdata = m_online_user_manager.findUserBySockfd(sockfd);
#else
    auto userdata = m_online_users.at(sockfd);
#endif

    return userdata;
}


} //namespace yy::server
