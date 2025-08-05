#include "FrontendServer.h"
#include "TcpConnection.h"
#include "UdpSession.h"
#include "log.h"
#include "connection.pb.h"
#include "md5/md5.h"
#include "UserConnection.h"
#include "EventLoop.h"
#include "Socket.h"

#include <google/protobuf/message.h>

using namespace yy::net;
using yy::core::UserConnection;
using yy::core::MessageHeader_Cmd;

namespace yy::core {

FrontendServer::FrontendServer(EventLoop *accpetorLoop, const IPAddressPtr& tcp_addr, const IPAddressPtr& udp_addr) :
    m_accpetorLoop{accpetorLoop},
    m_appConfigvar(config::g_app_config),
    m_tcpServer(accpetorLoop, tcp_addr, true,
        m_appConfigvar->GetValue().send_bytes_one(),
        m_appConfigvar->GetValue().send_bytes_max(),
        m_appConfigvar->GetValue().recv_bytes_one(),
        m_appConfigvar->GetValue().recv_bytes_max(),
        m_appConfigvar->GetValue().app_xor_code()),
    m_tcpDispatcher([this](const TcpConnectionPtr& conn, const MessagePtr& msg) {
        this->OnUnknownTcpMessage(conn, msg);
    }),
    m_tcpCodec([this](const TcpConnectionPtr& conn, const MessagePtr& msg) {
        m_tcpDispatcher.OnProtobufMessage(conn, msg);
    }),
    m_udpServer(accpetorLoop, udp_addr, true,
        m_appConfigvar->GetValue().recv_bytes_one(),
        m_appConfigvar->GetValue().udp_io_thread_num(),
        m_appConfigvar->GetValue().app_xor_code()),
    m_udpDispatcher([this](const UdpSessionPtr& conn, const MessagePtr& msg) {
        this->OnUnknownUdpMessage(conn, msg);
    }),
    m_udpCodec([this](const UdpSessionPtr& conn, const MessagePtr& msg) {
        m_udpDispatcher.OnProtobufMessage(conn, msg);
    })
{
    //! 消息回调注册
    m_tcpDispatcher.RegisterMessageCallback<protocol::core::HeartBody>( [this](const TcpConnectionPtr& conn, const HeartPtr& msg) { this->OnTcpHeart(conn, msg); });
    m_tcpDispatcher.RegisterMessageCallback<protocol::core::SecurityCheckReq>( [this](const TcpConnectionPtr& conn, const C2SSecurityPtr& msg) { this->OnSecurity(conn, msg); });
    m_tcpDispatcher.RegisterMessageCallback<protocol::core::UdpPortRegisterReq>( [this](const TcpConnectionPtr& conn, const UdpPortRegisterReqPtr& msg) { this->OnUdpPortRegisterRequest(conn, msg); });
    m_udpDispatcher.RegisterMessageCallback<protocol::core::HeartBody>( [this](const UdpSessionPtr& conn, const HeartPtr& msg) { this->OnUdpHeart(conn, msg); });

    m_tcpServer.SetMessageCallback(
        [this](const TcpConnectionPtr& conn, NetBuffer& buf) {
            m_tcpCodec.OnTcpData(conn, buf);
        });

    m_tcpServer.SetConnectionEstablishedCallback(
        [this](const TcpConnectionPtr& conn) {
            this->OnConnectionEstablished(conn);
        });

    m_tcpServer.SetConnectionShutdownCallback(
        [this](const TcpConnectionPtr& conn) {
            this->OnConnectionShutdown(conn);
        });

    m_udpServer.SetMessageCallback(
        [this](const UdpSessionPtr& conn, NetBuffer& buf) {
            m_udpCodec.OnData(conn, buf);
        });
}

FrontendServer::~FrontendServer()
{
    Stop();
}


void FrontendServer::Start(const F_ThreadInitCallback& cb) {
    m_tcpServer.Start(config::g_app_config->GetValue().tcp_io_thread_num(), 500ms, cb);
    m_udpServer.Start(1, 500ms);
}

void FrontendServer::Stop() {
    m_accpetorLoop->QuitLoop();
}


void FrontendServer::OnUnknownTcpMessage(const TcpConnectionPtr & conn, const MessagePtr &message) {
    YLOG_TRACE("Tcp消息：{}，交由业务层", message->GetDescriptor()->full_name());

    // 执行业务层回调，分发消息
    m_NotifierCommand(FindUser(conn->GetConnID()), message, MessageNetType::TCP);
}

void FrontendServer::OnUnknownUdpMessage(const UdpSessionPtr & sess, const MessagePtr &message) {
    YLOG_TRACE("Udp消息：{}，交由业务层", message->GetDescriptor()->full_name());

    // 执行业务层回调，分发消息
    m_NotifierCommand(FindUser(sess->GetConnID()), message, MessageNetType::UDP);
}

void FrontendServer::OnConnectionShutdown(const TcpConnectionPtr & conn) {
    //! 应用层处理
    if(m_NotifierDisconnect)
        m_NotifierDisconnect(FindUser(conn->GetConnID()));

    //! 核心层处理
    DelUser(conn->GetConnID());
}

void FrontendServer::OnConnectionEstablished(const TcpConnectionPtr  & conn) {
    YLOG_INFO("███████████████████连接成功<{}:{}, {}>！",
              conn->GetPeerAddr()->GetIPStr().c_str(), conn->GetPeerAddr()->GetPort(), conn->GetSocketFD());

    const auto userdata = std::make_shared<UserConnection>(conn, m_tcpCodec, m_udpCodec);
    AddUser(conn->GetConnID(), userdata);
    AddCheckTimer(conn, userdata);
    SendXorCode(conn);
}

void FrontendServer::AddCheckTimer(const TcpConnectionPtr & conn, const UserConnectionPtr & userdata) {
    /* ***** 需要是弱引用，不能因为这个回调函数延长TcpConnection的生命周期 ***** */
    //! ①检查是否在指定时间内完成安全连接的认证，若未认证，则关闭连接
    conn->GetIOLoop()->RunAfter(Seconds{GetAppConfig().time_security_max()},
        [weak_userdata = std::weak_ptr{userdata}]
        {
            if(auto userdata = weak_userdata.lock()) {
                if (userdata->IsConnected() and !userdata->IsSecure()) {
                    YLOG_WARN("<{},{}>主线程Update_CheckDisconnetion: 用户安全验证超时，关闭用户连接！", userdata->GetSocketFD(), userdata->GetConnID());
                    userdata->Shutdown();
                }
            }
        });

    //! ②检查是否收到心跳包，如未收到，则shutdown连接
    conn->GetIOLoop()->RunAfter(Seconds{GetAppConfig().time_heart_max()},
        [this, weak_userdata = std::weak_ptr{userdata}] {
            if(const auto user_connection = weak_userdata.lock()) {
                this->CheckHeart(user_connection);
            }
        });
}

void FrontendServer::CheckHeart(const UserConnectionPtr & userdata) {
    const auto & conn = userdata->GetConnection();
    if(!conn->IsConnected() or Timestamp::Now() - conn->GetHeartTime() > Seconds{GetAppConfig().time_heart_max()}) {
        YLOG_WARN("<{}>主线程Update_CheckDisconnetion: 用户心跳包超时，关闭用户连接！", conn->GetSocketFD());
        conn->Shutdown();
    } else {
        //! 使用RunAfter进行定时器的链式调用，若连接失效则不会续期定时器
        conn->GetIOLoop()->RunAfter(Seconds{GetAppConfig().time_heart_max()},
            [this, weak_userdata = std::weak_ptr{userdata}] //! 需要是弱引用，不能因为这个回调函数延长TcpConnection的生命周期
            {
                if(const auto user_connection = weak_userdata.lock()) {
                    this->CheckHeart(user_connection);
                }
            });
    }
}

void FrontendServer::SendXorCode(const TcpConnectionPtr &conn) {
    // 发送随机生成的异或码给用户，之后的通信都用该异或码进行加密
    uint8_t gen_val = util::GenerateXorCode();
    protocol::core::XorBodyRsp xorBody;
    xorBody.set_xor_code(gen_val ^ GetAppConfig().app_xor_code()); //! 记得与初始异或码异或

    m_tcpCodec.SendTCP(conn, xorBody);
    conn->SetXorCode(gen_val); //! 必须在Send后面

    YLOG_TRACE("Thread_Accepter: 封包异或码<{},{}>给用户<{}>，<{},{}>异或标识头<{},{}>", gen_val, xorBody.xor_code(), conn->GetSocketFD(),
               static_cast<int>(GetAppConfig().check_code()[0]), static_cast<int>(GetAppConfig().check_code()[1]),
               (int)(GetAppConfig().check_code()[0] ^ gen_val), (int)(GetAppConfig().check_code()[1] ^ gen_val)
   );
}




void FrontendServer::OnTcpHeart(const TcpConnectionPtr & conn, const HeartPtr & message) {
    assert(conn != nullptr);
    // YLOG_DEBUG("收到TCP心跳包");
    // 只需发一个只有消息头的包
    protocol::core::HeartBody heartBody;
    m_tcpCodec.SendTCP(conn, heartBody);
}

void FrontendServer::OnUdpHeart(const UdpSessionPtr & conn, const HeartPtr & message) {
    assert(conn != nullptr);
    // YLOG_DEBUG("收到UDP心跳包");
    // 只需发一个只有消息头的包
    protocol::core::HeartBody heartBody;
    m_udpCodec.SendUDP(conn, heartBody);
}

void FrontendServer::OnSecurity(const TcpConnectionPtr & conn, const C2SSecurityPtr & message)
{
    assert(conn != nullptr);

    char md5Arr[35]{};
    char Arr[30]{};
    snprintf(Arr, sizeof(Arr), "%s_%d", m_appConfigvar->GetValue().security_code(), conn->GetXorCode());
    md5::EncryptMD5str(md5Arr, reinterpret_cast<unsigned char*>(Arr), static_cast<int>(strlen(Arr)));

    YLOG_TRACE("服务器: {}, {}, {}", GetAppConfig().app_id(), GetAppConfig().app_version(), md5Arr)
    YLOG_TRACE("客户端: {}, {}, {}", message->app_id(),message->app_version(), message->app_md5().c_str())

    //! 进行安全验证
    protocol::core::SecurityCheckRsp::ResultCode resultCode;
    if(message->app_version() != GetAppConfig().app_version()) {
        YLOG_DEBUG("<{}>解包执行：版本不同，安全验证失败！", conn->GetConnID())
        resultCode = protocol::core::SecurityCheckRsp_ResultCode_eAppVersionFailed;
    }
    else if(util::StrCmp_IgnoreCase(message->app_md5().c_str(), md5Arr)) {
        YLOG_DEBUG("<{}>解包执行：md5码不同，安全验证失败！", conn->GetConnID())
        resultCode = protocol::core::SecurityCheckRsp_ResultCode_eMd5Failed;
    }
    else {
        resultCode = protocol::core::SecurityCheckRsp_ResultCode_eSuccess;
    }

    //! 安全验证通过：交由业务层
    if(resultCode == protocol::core::SecurityCheckRsp_ResultCode_eSuccess) {
        //! 发送安全验证结果
        protocol::core::SecurityCheckRsp resultBody;
        resultBody.set_result_code(resultCode);
        resultBody.set_server_udp_port(m_udpServer.GetRecvAddr()->GetPort());
        resultBody.set_session_id(conn->GetConnID());
        m_tcpCodec.SendTCP(conn, resultBody);

        const auto userconn = FindUser(conn->GetConnID());
        userconn->SetState(UserConnection::E_UserBaseState::eSecure);
        if(m_NotifierSecurity)
            m_NotifierSecurity(userconn);
        YLOG_TRACE("<{}>解包执行：安全验证通过", conn->GetConnID())
    }
    //! 安全验证失败：关闭用户连接
    else {
        conn->Shutdown();
        YLOG_TRACE("<{}>解包执行：用户安全验证失败！", conn->GetConnID())
    }
}

void FrontendServer::OnUdpPortRegisterRequest(const TcpConnectionPtr & conn, const UdpPortRegisterReqPtr & message)
{
    if(message->session_id() != conn->GetConnID()) {
        YLOG_INFO("<{}>客户端会话ID验证错误", conn->GetConnID())
        conn->Shutdown();
    }

    std::string client_ip = message->client_udp_ip();
    uint32_t client_port = message->client_udp_port();
    IPAddressPtr udpAddr = std::make_shared<IPv4Address>(client_ip, client_port);
    YLOG_INFO("<{}>客户端Udp地址[{}:{}]", conn->GetConnID(), client_ip, client_port);

    const UdpSessionPtr udpSession = std::make_unique<UdpSession>(conn->GetConnID(), m_udpServer.GetUdpTran(), udpAddr, m_appConfigvar->GetValue().app_xor_code());
    FindUser(conn->GetConnID())->BindUdp(udpSession);

    protocol::core::UdpPortRegisterRsp response;
    response.set_session_id(conn->GetConnID());
    response.set_status(protocol::core::UdpPortRegisterRsp_Status_eSuccess);
    m_tcpCodec.SendTCP(conn, response);
}





TimerID FrontendServer::RunAt(const Timestamp time, F_TaskCallback cb) {
    return m_accpetorLoop->RunAt(time, std::move(cb));
}

TimerID FrontendServer::RunAfter(const Microseconds delay, F_TaskCallback cb) {
    return m_accpetorLoop->RunAfter(delay, std::move(cb));
}

TimerID FrontendServer::RunEvery(const Microseconds interval, F_TaskCallback cb) {
    return m_accpetorLoop->RunEvery(interval, std::move(cb));
}

void FrontendServer::CancelTimer(const TimerID timerid) {
    m_accpetorLoop->CancelTimer(timerid);
}







UserConnectionPtr FrontendServer::FindUser(const uint64_t conn_id) {
    util::ReadLockGuard lg{m_usersMutex};

    const auto it = m_users.find(conn_id);
    return (it == m_users.end()) ? nullptr : it->second;
}

void FrontendServer::DelUser(const uint64_t conn_id) {
    util::WriteLockGuard lg{m_usersMutex};

    const auto it = m_users.find(conn_id);
    if(it != m_users.end()) {
        it->second->SetState(UserConnection::E_UserBaseState::eFree);
        m_users.erase(it);
    }
}

void FrontendServer::AddUser(const uint64_t conn_id, const UserConnectionPtr & userdata) {
    util::WriteLockGuard lg{m_usersMutex};

    m_users[conn_id] = userdata;
}



}