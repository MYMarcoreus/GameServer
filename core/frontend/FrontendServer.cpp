#include "FrontendServer.h"
#include "TcpConnection.h"
#include "log.h"
#include "connection.pb.h"
#include "md5/md5.h"
#include "UserConnection.h"
#include "EventLoop.h"
#include "ProtobufTcpCodec_Cmd.h"
#include "ProtobufUdpCodec_Cmd.h"
#include "TcpServer.h"
#include "UdpServer.h"
#include "UdpSession.h"
#include <google/protobuf/message.h>

#include "game.pb.h"

using namespace yy::net;
using yy::core::UserConnection;
using yy::core::MessageHeader_Cmd;

namespace yy::core {

FrontendServer::FrontendServer(EventLoop *accpetorLoop, const IPAddressPtr& tcp_addr, const IPAddressPtr& udp_addr) :
    m_accpetorLoop{accpetorLoop},
    m_appConfigvar(config::g_app_config),
    m_tcpServer(std::make_unique<TcpServer>(accpetorLoop, tcp_addr, true,
        m_appConfigvar->GetValue().send_bytes_one(),
        m_appConfigvar->GetValue().send_bytes_max(),
        m_appConfigvar->GetValue().recv_bytes_one(),
        m_appConfigvar->GetValue().recv_bytes_max(),
        m_appConfigvar->GetValue().app_xor_code())
    ),
    m_tcpDispatcher([this](const TcpConnectionPtr& conn, const MessagePtr& msg) {
        this->OnUnknownTcpMessage(conn, msg);
    }),
    m_tcpCodec(std::make_unique<ProtobufTcpCodec>([this](const TcpConnectionPtr& conn, const MessagePtr& msg) {
        m_tcpDispatcher.OnProtobufMessage(conn, msg);
    })),
    m_udpServer(std::make_unique<UdpServer>(accpetorLoop, udp_addr, true,
        m_appConfigvar->GetValue().recv_bytes_one(),
        m_appConfigvar->GetValue().udp_io_thread_num(),
        m_appConfigvar->GetValue().app_xor_code())
    ),
    m_udpDispatcher([this](const UdpSessionPtr& conn, const MessagePtr& msg) {
        this->OnUnknownUdpMessage(conn, msg);
    }),
    m_udpCodec(std::make_unique<ProtobufUdpCodec>([this](const UdpSessionPtr& conn, const MessagePtr& msg) {
        m_udpDispatcher.OnProtobufMessage(conn, msg);
    }))
{
    //! 消息回调注册
    m_tcpDispatcher.RegisterMessageCallback<protocol::core::HeartBody>( [this](const TcpConnectionPtr& conn, const HeartPtr& msg) { this->OnTcpHeart(conn, msg); });
    m_tcpDispatcher.RegisterMessageCallback<protocol::core::SecurityCheckReq>( [this](const TcpConnectionPtr& conn, const C2SSecurityPtr& msg) { this->OnSecurity(conn, msg); });
    m_tcpDispatcher.RegisterMessageCallback<protocol::core::UdpPortRegisterReq>( [this](const TcpConnectionPtr& conn, const UdpPortRegisterReqPtr& msg) { this->OnUdpPortRegisterRequest(conn, msg); });
    m_udpDispatcher.RegisterMessageCallback<protocol::core::HeartBody>( [this](const UdpSessionPtr& conn, const HeartPtr& msg) { this->OnUdpHeart(conn, msg); });

    m_tcpServer->SetMessageCallback(
        [this](const TcpConnectionPtr& conn, NetBuffer& buf) {
            m_tcpCodec->OnTcpData(conn, buf);
        });

    m_tcpServer->SetConnectionEstablishedCallback(
        [this](const TcpConnectionPtr& conn) {
            this->OnConnectionEstablished(conn);
        });

    m_tcpServer->SetConnectionShutdownCallback(
        [this](const TcpConnectionPtr& conn) {
            this->OnConnectionShutdown(conn);
        });

    m_udpServer->SetMessageCallback(
        [this](const UdpSessionPtr& conn, NetBuffer& buf) {
            m_udpCodec->OnData(conn, buf);
        });
}

FrontendServer::~FrontendServer()
{
    Stop();
}


void FrontendServer::Start(const F_ThreadInitCallback& cb) {
    m_tcpServer->Start(static_cast<int>(config::g_app_config->GetValue().tcp_io_thread_num()), 500ms, cb);
    m_udpServer->Start(1, 500ms);
}

void FrontendServer::Stop() {
    m_accpetorLoop->QuitLoop();
}

bool FrontendServer::IsRunning() const
{ return m_tcpServer->IsRunning(); }

const config::AppXmlConfig& FrontendServer::GetAppConfig()
{ return m_appConfigvar->GetValue(); }

auto FrontendServer::GetTcpListenAddr() const -> net::IPAddressPtr
{ return m_tcpServer->GetListenAddr(); }


void FrontendServer::OnUnknownTcpMessage(const TcpConnectionPtr & conn, const MessagePtr &message) {
    YLOG_TRACE("Tcp消息：{}，交由业务层", message->GetDescriptor()->full_name());
    const auto userconn = FindUser(conn->GetConnID());
    // 执行业务层回调，分发消息
    m_NotifierCommand(userconn, message, MessageNetType::TCP);
}

void FrontendServer::OnUnknownUdpMessage(const UdpSessionPtr & sess, const MessagePtr &message) {
    YLOG_TRACE("Udp消息：{}，交由业务层", message->GetDescriptor()->full_name());
    const auto userconn = FindUser(sess->GetConnID());
#ifdef ____DEBUG
    if (message->GetDescriptor()->name() == "C2SMove") {
        const auto move = dynamic_cast<protocol::app::C2SMove*>(message.get());
        if (move and move->uid() != userconn->GetUID()) {
            printf("错误！");
        }
    }
#endif
    // 执行业务层回调，分发消息
    m_NotifierCommand(userconn, message, MessageNetType::UDP);
}

void FrontendServer::OnConnectionShutdown(const TcpConnectionPtr & conn) {
    //! 应用层处理
    if(m_NotifierDisconnect)
        m_NotifierDisconnect(FindUser(conn->GetConnID()));

    //! 核心层处理
    DelUser(conn->GetConnID());

    // 网络层处理
    m_udpServer->UnregisterSession(conn->GetConnID());
}

void FrontendServer::OnConnectionEstablished(const TcpConnectionPtr  & conn) {
    YLOG_INFO("███████████████████连接成功<{}:{}, {}>！",
              conn->GetPeerAddr()->GetIPStr(), conn->GetPeerAddr()->GetPort(), conn->GetSocketFD());

    const auto userconn = std::make_shared<UserConnection>(conn, *m_tcpCodec, *m_udpCodec);
    AddUser(conn->GetConnID(), userconn);
    AddCheckTimer(conn, userconn);
    SendXorCode(conn);
}

void FrontendServer::AddCheckTimer(const TcpConnectionPtr & conn, const UserConnectionPtr & userconn) {
    /* ***** 需要是弱引用，不能因为这个回调函数延长TcpConnection的生命周期 ***** */
    //! ①检查是否在指定时间内完成安全连接的认证，若未认证，则关闭连接
    conn->GetIOLoop()->RunAfter(Seconds{GetAppConfig().time_security_max()},
        [weak_userconn = std::weak_ptr{userconn}]
        {
            if(const auto userconn_sp = weak_userconn.lock()) {
                if (userconn_sp->IsConnected() and !userconn_sp->IsSecure()) {
                    YLOG_WARN("<{}>主线程Update_CheckDisconnetion: 用户安全验证超时，关闭用户连接！", userconn_sp->GetConnID());
                    userconn_sp->Shutdown();
                }
            }
        });

    //! ②检查是否收到心跳包，如未收到，则shutdown连接
    conn->GetIOLoop()->RunAfter(Seconds{GetAppConfig().time_heart_max()},
        [this, weak_userconn = std::weak_ptr{userconn}] {
            if(const auto userconn_sp = weak_userconn.lock()) {
                this->CheckHeart(userconn_sp);
            }
        });
}

void FrontendServer::CheckHeart(const UserConnectionPtr & userconn) {
    if(!userconn->IsConnected() or Timestamp::Now() - userconn->GetHeartTime() > Seconds{GetAppConfig().time_heart_max()}) {
        YLOG_WARN("<{}>主线程Update_CheckDisconnetion: 用户心跳包超时，关闭用户连接！", userconn->GetConnID());
        userconn->Shutdown();
    } else {
        //! 使用RunAfter进行定时器的链式调用，若连接失效则不会续期定时器
        userconn->RunAfter(Seconds{GetAppConfig().time_heart_max()},
            [this, weak_userconn = std::weak_ptr{userconn}] //! 需要是弱引用，不能因为这个回调函数延长TcpConnection的生命周期
            {
                if(const auto userconn_sp = weak_userconn.lock()) {
                    this->CheckHeart(userconn_sp);
                }
            });
    }
}

void FrontendServer::SendXorCode(const TcpConnectionPtr &conn) {
    // 发送随机生成的异或码给用户，之后的通信都用该异或码进行加密
    uint8_t gen_val = util::GenerateXorCode();
    protocol::core::XorBodyRsp xorBody;
    xorBody.set_xor_code(gen_val ^ GetAppConfig().app_xor_code()); //! 记得与初始异或码异或

    m_tcpCodec->SendTCP(conn, xorBody);
    conn->SetXorCode(gen_val); //! 必须在Send后面

    YLOG_TRACE("Thread_Accepter: 封包异或码<{},{}>给用户<{}>，<{},{}>异或标识头<{},{}>", gen_val, xorBody.xor_code(), conn->GetSocketFD(),
               static_cast<int>(GetAppConfig().check_code()[0]), static_cast<int>(GetAppConfig().check_code()[1]),
               (int)(GetAppConfig().check_code()[0] ^ gen_val), (int)(GetAppConfig().check_code()[1] ^ gen_val)
   );
}




void FrontendServer::OnTcpHeart(const TcpConnectionPtr & conn, const HeartPtr & message) {
    assert(conn != nullptr);
    FindUser(conn->GetConnID())->UpdateHeartTime();
    protocol::core::HeartBody heartBody;
    m_tcpCodec->SendTCP(conn, heartBody);
}

void FrontendServer::OnUdpHeart(const UdpSessionPtr & conn, const HeartPtr & message) {
    assert(conn != nullptr);
    FindUser(conn->GetConnID())->UpdateHeartTime();
    protocol::core::HeartBody heartBody;
    m_udpCodec->SendUDP(conn, heartBody);
}

void FrontendServer::OnSecurity(const TcpConnectionPtr & conn, const C2SSecurityPtr & message)
{
    assert(conn != nullptr);

    char md5Arr[35]{};
    char Arr[30]{};
    snprintf(Arr, sizeof(Arr), "%s_%d", m_appConfigvar->GetValue().security_code(), conn->GetXorCode());
    md5::EncryptMD5str(md5Arr, reinterpret_cast<unsigned char*>(Arr), static_cast<int>(strlen(Arr)));

    YLOG_TRACE("服务器: {}, {}, {}", GetAppConfig().app_id(), GetAppConfig().app_version(), md5Arr)
    YLOG_TRACE("客户端: {}, {}, {}", message->app_id(),message->app_version(), message->app_md5())

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
        resultBody.set_server_udp_port(m_udpServer->GetRecvAddr()->GetPort());
        resultBody.set_session_id(conn->GetConnID());
        m_tcpCodec->SendTCP(conn, resultBody);

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
    const IPAddressPtr udpAddr = std::make_shared<IPv4Address>(client_ip, client_port);
    YLOG_INFO("<{}>客户端Udp地址[{}:{}]", conn->GetConnID(), client_ip, client_port);

    const UdpSessionPtr udpSession = m_udpServer->RegisterSession(conn->GetConnID(), udpAddr);
    const auto userconn = FindUser(conn->GetConnID());
    userconn->BindUdp(udpSession);

    // 注册1s一次的udp心跳包
    userconn->RunEvery(1s,
        [this, weak_udpSession = std::weak_ptr{udpSession}] //! 需要是弱引用，不能因为这个回调函数延长udpSession的生命周期
        {
            if(const auto session = weak_udpSession.lock()) {
                const protocol::core::HeartBody heartBody;
                m_udpCodec->SendUDP(session, heartBody);
            }
        });

    protocol::core::UdpPortRegisterRsp response;
    response.set_session_id(conn->GetConnID());
    response.set_status(protocol::core::UdpPortRegisterRsp_Status_eSuccess);
    m_tcpCodec->SendTCP(conn, response);
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

void FrontendServer::AddUser(const uint64_t conn_id, const UserConnectionPtr & userconn) {
    util::WriteLockGuard lg{m_usersMutex};

    m_users[conn_id] = userconn;
}



}