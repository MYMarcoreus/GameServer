#include "BackendServer.h"
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
using yy::core::MessageHeader;

namespace yy::core {

BackendServer::BackendServer(EventLoop *accpetorLoop, const IPAddressPtr& listenAddr) :
    m_appConfigvar(config::g_app_config),
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
    //! 消息回调注册
    m_tcpDispatcher.RegisterMessageCallback<protocol::core::HeartBody>( [this](const TcpConnectionPtr& conn, const HeartPtr& msg) { this->OnTcpHeart(conn, msg); });
    m_tcpDispatcher.RegisterMessageCallback<protocol::core::C2SUdpPortRegister>( [this](const TcpConnectionPtr& conn, const C2SUdpPortRegisterPtr& msg) { this->OnUdpPortRegisterRequest(conn, msg); });
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
            this->AfterShutdownConnection(conn);
        });

    m_udpServer.SetMessageCallback(
        [this](const UdpSessionPtr& conn, NetBuffer& buf) {
            m_udpCodec.OnData(conn, buf);
        });
}


BackendServer::~BackendServer() {
    Stop();
}

void BackendServer::Start(const F_ThreadInitCallback& cb) {
    m_tcpServer.Start(config::g_app_config->GetValue().tcp_io_thread_num(), 500ms, cb);
    m_udpServer.Start(1, 500ms);
}

void BackendServer::Stop() {
    m_accpetorLoop->QuitLoop();
}


void BackendServer::OnUnknownTcpMessage(const TcpConnectionPtr & conn, const MessagePtr &message) {
    YLOG_TRACE("Tcp消息：{}，交由业务层", message->GetDescriptor()->full_name());

    // 执行业务层回调，分发消息
    m_NotifierCommand(FindUser(conn->GetConnID()), message, MessageType::TCP);
}

void BackendServer::OnUnknownUdpMessage(const UdpSessionPtr & sess, const MessagePtr &message) {
    YLOG_TRACE("Udp消息：{}，交由业务层", message->GetDescriptor()->full_name());

    // 执行业务层回调，分发消息
    m_NotifierCommand(FindUser(sess->GetConnID()), message, MessageType::UDP);
}

void BackendServer::OnConnectionEstablished(const TcpConnectionPtr  & conn) {
    YLOG_INFO("███████████████████连接成功<{}:{}, {}>！",
              conn->GetPeerAddr()->GetIPStr().c_str(), conn->GetPeerAddr()->GetPort(), conn->GetSocketFD());

    auto userdata = std::make_shared<UserConnection>(conn, m_tcpCodec, m_udpCodec);
    AddUser(conn->GetConnID(), userdata);
    AddCheckTimer(conn, userdata);

    ++m_numSecurity;
    if(m_NotifierSecurity)
        m_NotifierSecurity(FindUser(conn->GetConnID()));
}

void BackendServer::AddCheckTimer(const TcpConnectionPtr & conn, const UserConnectionPtr & userdata) {
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

void BackendServer::CheckHeart(const UserConnectionPtr & userdata) {
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


void BackendServer::OnTcpHeart(const TcpConnectionPtr & conn, const HeartPtr & message) {
    assert(conn != nullptr);
    YLOG_DEBUG("收到TCP心跳包");
    // 只需发一个只有消息头的包
    protocol::core::HeartBody heartBody;
    m_tcpCodec.SendTCP(conn, heartBody);
}

void BackendServer::OnUdpHeart(const UdpSessionPtr & conn, const HeartPtr & message) {
    assert(conn != nullptr);
    YLOG_DEBUG("收到UDP心跳包");
    // 只需发一个只有消息头的包
    protocol::core::HeartBody heartBody;
    m_udpCodec.SendUDP(conn, heartBody);
}

void BackendServer::OnUdpPortRegisterRequest(const TcpConnectionPtr & conn, const C2SUdpPortRegisterPtr & message)
{
    if(message->session_id() != conn->GetConnID()) {
        YLOG_INFO("<{}>客户端会话ID验证错误", conn->GetConnID())
        conn->Shutdown();
    }

    auto client_ip   = message->client_udp_ip();
    auto client_port = message->client_udp_port();
    IPAddressPtr udpAddr = std::make_shared<IPv4Address>(client_ip, client_port);
    YLOG_INFO("<{}>客户端Udp地址[{}:{}]", conn->GetConnID(), client_ip, client_port);

    UdpSessionPtr udpSession = std::make_unique<UdpSession>(conn->GetConnID(), m_udpServer.GetUdpTran(), udpAddr, m_appConfigvar->GetValue().app_xor_code());
    FindUser(conn->GetConnID())->BindUdp(udpSession);

    protocol::core::S2CUdpPortRegister response;
    response.set_session_id(conn->GetConnID());
    response.set_status(protocol::core::S2CUdpPortRegister_Status_eSuccess);
    m_tcpCodec.SendTCP(conn, response);
}





TimerID BackendServer::RunAt(Timestamp time, F_TaskCallback cb) {
    return m_accpetorLoop->RunAt(time, std::move(cb));
}

TimerID BackendServer::RunAfter(Microseconds delay, F_TaskCallback cb) {
    return m_accpetorLoop->RunAfter(delay, std::move(cb));
}

TimerID BackendServer::RunEvery(Microseconds interval, F_TaskCallback cb) {
    return m_accpetorLoop->RunEvery(interval, std::move(cb));
}

void BackendServer::CancelTimer(TimerID timerid) {
    m_accpetorLoop->CancelTimer(timerid);
}



void BackendServer::AfterShutdownConnection(const TcpConnectionPtr & conn) {
    //! 应用层处理
    if(m_NotifierDisconnect)
        m_NotifierDisconnect(FindUser(conn->GetConnID()));
}



UserConnectionPtr BackendServer::FindUser(const uint64_t conn_id) {
    util::ReadLockGuard lg{m_usersMutex};

    const auto it = m_users.find(conn_id);
    return (it == m_users.end()) ? nullptr : it->second;
}

void BackendServer::DelUser(const uint64_t conn_id) {
    util::WriteLockGuard lg{m_usersMutex};

    m_users.erase(conn_id);
}

void BackendServer::AddUser(const uint64_t conn_id, const UserConnectionPtr & userdata) {
    util::WriteLockGuard lg{m_usersMutex};

    m_users[conn_id] = userdata;
}



}