#include "LogicServer.h"
#include "TcpConnection.h"
#include "UdpSession.h"
#include "log.h"
#include "connection.pb.h"
#include "md5/md5.h"
#include "UserConnection.h"
#include "EventLoop.h"
#include "Socket.h"
#include "RemoteXmlConfig.h"
#include "MySqlPool.h"
#include "RedisClient.h"

#include <google/protobuf/message.h>

using namespace yy::net;
using yy::core::UserConnection;
using yy::core::MessageHeader;

namespace yy::app {

LogicServer::LogicServer(EventLoop *accpetorLoop, const IPAddressPtr& listenAddr) :
    m_appConfigvar(yy::config::g_app_config),
    m_accpetorLoop{accpetorLoop},
    m_tcpServer(accpetorLoop, listenAddr, true,
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
    m_udpServer(accpetorLoop, true,
        m_appConfigvar->GetValue().app_udp_port(),
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
    //! TCP消息回调注册
    m_tcpDispatcher.RegisterMessageCallback<yy::protocol::core::HeartBody>( [this](const TcpConnectionPtr& conn, const HeartPtr& msg) { this->OnTcpHeart(conn, msg); });
    m_tcpDispatcher.RegisterMessageCallback<yy::protocol::core::C2SSecurityBody>( [this](const TcpConnectionPtr& conn, const C2SSecurityPtr& msg) { this->OnSecurity(conn, msg); });
    m_tcpDispatcher.RegisterMessageCallback<yy::protocol::core::C2SUdpPortRegister>( [this](const TcpConnectionPtr& conn, const C2SUdpPortRegisterPtr& msg) { this->OnC2SUdpPortRegister(conn, msg); });

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
            this->AfterShutdownConnection(conn);
        });

    //! UDP消息回调注册
    m_udpDispatcher.RegisterMessageCallback<yy::protocol::core::HeartBody>( [this](const UdpSessionPtr& conn, const HeartPtr& msg) { this->OnUdpHeart(conn, msg); });

    m_udpServer.SetMessageCallback(
        [this](const UdpSessionPtr& conn, NetBuffer& buf) {
            m_udpCodec.OnData(conn, buf);
        });
}


LogicServer::~LogicServer() {
    Stop();
}

void LogicServer::Start() {
    m_tcpServer.Start(config::g_app_config->GetValue().tcp_io_thread_num(), 500ms);
    m_udpServer.Start(1, 500ms);
}

void LogicServer::Stop() {
    m_accpetorLoop->QuitLoop();
}


void LogicServer::OnUnknownTcpMessage(const TcpConnectionPtr & conn, const MessagePtr &message) {
    YLOG_TRACE("游戏消息：{}，交由业务层", message->GetDescriptor()->full_name());

    // 执行业务层回调，分发消息
    m_NotifierCommand(FindUser(conn->GetConnID()), message);
}

void LogicServer::OnUnknownUdpMessage(const UdpSessionPtr & conn, const MessagePtr &message) {
    YLOG_TRACE("游戏消息：{}，交由业务层", message->GetDescriptor()->full_name());

    // 执行业务层回调，分发消息
    m_NotifierCommand(FindUser(conn->GetName()), message);
}

void LogicServer::OnConnectionEstablished(const TcpConnectionPtr  & conn) {
    YLOG_INFO("███████████████████连接成功<{}:{}, {}>！",
              conn->GetPeerAddr()->GetIPStr().c_str(), conn->GetPeerAddr()->GetPort(), conn->GetSocketFD());

    auto userdata = std::make_shared<UserConnection>(conn, m_tcpCodec, m_udpCodec);
    AddUser(conn->GetConnID(), userdata);
    AddCheckTimer(conn, userdata);
    SendXorCode(conn);
}

void LogicServer::AddCheckTimer(const TcpConnectionPtr & conn, const UserConnectionPtr & userdata) {
    /* ***** 需要是弱引用，不能因为这个回调函数延长TcpConnection的生命周期 ***** */
    //! ①检查是否在指定时间内完成安全连接的认证，若未认证，则关闭连接
    conn->GetIOLoop()->RunAfter(Seconds{GetAppConfig().time_security_max()},
                                [weak_userdata = std::weak_ptr<UserConnection>{userdata}]()
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
                                [this, weak_userdata = std::weak_ptr<UserConnection>{userdata}]()
    {
        if(const auto user_connection = weak_userdata.lock()) {
            this->CheckHeart(user_connection);
        }
    });
}

void LogicServer::CheckHeart(const UserConnectionPtr & userdata) {
    const auto & conn = userdata->GetConnection();
    if(!conn->IsConnected() or Timestamp::Now() - conn->GetHeartTime() > Seconds{m_appConfigvar->GetValue().time_heart_max()}) {
        YLOG_WARN("<{}>主线程Update_CheckDisconnetion: 用户心跳包超时，关闭用户连接！", conn->GetSocketFD());
        conn->Shutdown();
        // conn->GetLoop()->CancelTimer(this->m_heartTimerID); //! 不生效因为执行该函数时Timer不在列表中，执行完才加入列表
    } else {
        //! 需要是弱引用，不能因为这个回调函数延长TcpConnection的生命周期
        conn->GetIOLoop()->RunAfter(Seconds{GetAppConfig().time_heart_max()},
                                    [this, weak_userdata = std::weak_ptr<UserConnection>{userdata}]
        {
            if(const auto user_connection = weak_userdata.lock()) {
                this->CheckHeart(user_connection);
            }
        });
    }
}

void LogicServer::SendXorCode(const TcpConnectionPtr &conn) {
    // 发送随机生成的异或码给用户，之后的通信都用该异或码进行加密
    auto gen_val = MessageHeader::GenerateXorCode();
    yy::protocol::core::S2CXorBody xorBody;
    xorBody.set_xor_code(gen_val ^ GetAppConfig().app_xor_code()); //! 记得与初始异或码异或

    m_tcpCodec.SendTCP(conn, xorBody);
    conn->SetXorCode(gen_val); //! 必须在Send后面

    YLOG_TRACE("Thread_Accepter: 封包异或码<{},{}>给用户<{}>，<{},{}>异或标识头<{},{}>", gen_val, xorBody.xor_code(), conn->GetSocketFD(),
               static_cast<int>(GetAppConfig().check_code()[0]), static_cast<int>(GetAppConfig().check_code()[1]),
               (int)(GetAppConfig().check_code()[0] ^ gen_val), (int)(GetAppConfig().check_code()[1] ^ gen_val)
   );
}




void LogicServer::OnTcpHeart(const TcpConnectionPtr & conn, const HeartPtr & message) {
    assert(conn != nullptr);
    YLOG_DEBUG("收到TCP心跳包");
    // 只需发一个只有消息头的包
    yy::protocol::core::HeartBody heartBody;
    m_tcpCodec.SendTCP(conn, heartBody);
}

void LogicServer::OnUdpHeart(const UdpSessionPtr & conn, const HeartPtr & message) {
    assert(conn != nullptr);
    YLOG_DEBUG("收到UDP心跳包");
    // 只需发一个只有消息头的包
    yy::protocol::core::HeartBody heartBody;
    m_udpCodec.SendUDP(conn, heartBody);
}

void LogicServer::OnSecurity(const TcpConnectionPtr & conn, const C2SSecurityPtr & message)
{
    assert(conn != nullptr);

    char md5Arr[35]{};
    char Arr[30]{};
    snprintf(Arr, sizeof(Arr), "%s_%d", m_appConfigvar->GetValue().security_code(), conn->GetXorCode());
    ::md5::EncryptMD5str(md5Arr, reinterpret_cast<unsigned char*>(Arr), static_cast<int>(strlen(Arr)));

    YLOG_DEBUG("服务器: {}, {}, {}", GetAppConfig().app_id(), GetAppConfig().app_version(), md5Arr)
    YLOG_DEBUG("客户端: {}, {}, {}", message->app_id(),message->app_version(), message->app_md5().c_str())

    //! 进行安全验证
    yy::protocol::core::S2CSecurityBody::ResultCode resultCode;
    if(message->app_version() != GetAppConfig().app_version()) {
        YLOG_DEBUG("<{}>解包执行：版本不同，安全验证失败！", conn->GetConnID())
        resultCode = yy::protocol::core::S2CSecurityBody_ResultCode_eAppVersionFailed;
    }
    else if(util::StrCmp_IgnoreCase(message->app_md5().c_str(), md5Arr)) {
        YLOG_DEBUG("<{}>解包执行：md5码不同，安全验证失败！", conn->GetConnID())
        resultCode = yy::protocol::core::S2CSecurityBody_ResultCode_eMd5Failed;
    }
    else {
        resultCode = yy::protocol::core::S2CSecurityBody_ResultCode_eSuccess;
    }

    //! 发送安全验证结果
    yy::protocol::core::S2CSecurityBody resultBody;
    resultBody.set_result_code(resultCode);
    resultBody.set_server_udp_port(FindUser(conn->GetConnID())->GetSocketFD());
    resultBody.set_session_id(conn->GetConnID());
    m_tcpCodec.SendTCP(conn, resultBody);

    //! 安全验证通过：交由业务层
    if(resultBody.result_code() == yy::protocol::core::S2CSecurityBody_ResultCode_eSuccess) {
        ++m_numSecurity;
        if(m_NotifierSecurity)
            m_NotifierSecurity(FindUser(conn->GetConnID()));
        YLOG_INFO("<{}>解包执行：安全验证通过", conn->GetConnID())
    }
    //? 安全验证失败：需要关闭用户连接吗？
    else {
        conn->Shutdown();
        YLOG_INFO("<{}>解包执行：用户安全验证失败！", conn->GetConnID())
    }
}

void LogicServer::OnC2SUdpPortRegister(const TcpConnectionPtr & conn, const C2SUdpPortRegisterPtr & message)
{
    if(message->session_id() != conn->GetConnID()) {
        YLOG_INFO("<{}>客户端会话ID验证错误", conn->GetConnID())
        conn->Shutdown();
    }

    auto client_ip   = message->client_udp_ip();
    auto client_port = message->client_udp_port();
    net::IPAddressPtr udpAddr = std::make_shared<net::IPv4Address>(client_ip, client_port);
    YLOG_INFO("<{}>客户端Udp地址[{}:{}]", conn->GetConnID(), client_ip, client_port);

    UdpSessionPtr udpSession = std::make_unique<net::UdpSession>(conn->GetConnID(), m_udpServer.GetUdpTran(), udpAddr, m_appConfigvar->GetValue().app_xor_code());
    FindUser(conn->GetConnID())->BindUdp(udpSession);

    yy::protocol::core::S2CUdpPortRegister response;
    response.set_status(protocol::core::S2CUdpPortRegister_Status_eSuccess); // S2CUdpPortRegister_Status_eSuccess
    m_tcpCodec.SendTCP(conn, response);
}





net::TimerID LogicServer::RunAt(net::Timestamp time, net::F_TaskCallback cb) {
    return m_accpetorLoop->RunAt(time, std::move(cb));
}

net::TimerID LogicServer::RunAfter(net::Microseconds delay, net::F_TaskCallback cb) {
    return m_accpetorLoop->RunAfter(delay, std::move(cb));
}

net::TimerID LogicServer::RunEvery(net::Microseconds interval, net::F_TaskCallback cb) {
    return m_accpetorLoop->RunEvery(interval, std::move(cb));
}

void LogicServer::CancelTimer(net::TimerID timerid) {
    m_accpetorLoop->CancelTimer(timerid);
}



void LogicServer::AfterShutdownConnection(const TcpConnectionPtr & conn) {
    //! 应用层处理
    if(m_NotifierDisconnect)
        m_NotifierDisconnect(FindUser(conn->GetConnID()));
}



UserConnectionPtr LogicServer::FindUser(uint64_t conn_id) {
    std::lock_guard lg{m_usersMutex};

    const auto it = m_users.find(conn_id);
    return (it == m_users.end()) ? nullptr : it->second;
}

void LogicServer::DelUser(uint64_t conn_id) {
    std::lock_guard lg{m_usersMutex};

    m_users.erase(conn_id);
}

void LogicServer::AddUser(uint64_t conn_id, const UserConnectionPtr & userdata) {
    std::lock_guard lg{m_usersMutex};

    m_users[conn_id] = userdata;
}



}
