#include "WindowsGameServer.h"
#include "TcpConnection.h"
#include "log.h"
#include "connection.pb.h"
#include "md5/md5.h"
#include "UserBaseData.h"

#include <google/protobuf/message.h>

using namespace yy::net;
using namespace yy::config;

namespace yy::core {


WindowsGameServer::WindowsGameServer(EventLoop *loop, IPAddressPtr listenAddr)
        : m_loop{loop},
          m_server(loop, listenAddr, true),
          m_dispatcher( std::bind(&WindowsGameServer::OnUnknownMessage, this, _1, _2) ),
          m_codec(std::bind(&ProtobufDispatcher::OnProtobufMessage, &m_dispatcher, _1, _2)),
          m_app_configvar(g_app_config)
{
    m_dispatcher.RegisterMessageCallback<protocol::HeartBody>(std::bind(&WindowsGameServer::OnHeart, this, _1, _2));
    m_dispatcher.RegisterMessageCallback<protocol::SecurityBody>(std::bind(&WindowsGameServer::OnSecurity, this, _1, _2));
    m_server.SetMessageCallback( std::bind(&ProtobufCodec::OnMessage, &m_codec, _1, _2));
    m_server.SetConnectionEstablishedCallback( std::bind(&WindowsGameServer::OnConnectionEstablished, this, _1));
    m_server.SetConnectionShutdownCallback([this](const TcpConnectionPtr & conn) { this->AddShutdownConnection(conn); });
    m_server.SetCloseSocketsCallback([this]() { this->CheckDisconnections(); });
}

WindowsGameServer::~WindowsGameServer() {

}



void WindowsGameServer::OnUnknownMessage(TcpConnectionPtr conn, const MessagePtr &message) {
    YLOG_INFO("游戏消息：{}", message->GetDescriptor()->full_name());
}




void WindowsGameServer::OnConnectionEstablished(TcpConnectionPtr conn) {
    YLOG_INFO("███████████████████连接成功<{}:{}, {}>！",
              conn->GetPeerAddr()->GetIPStr().c_str(), conn->GetPeerAddr()->GetPort(), conn->GetSocketFD());

    SendXorCode(conn);
}

void WindowsGameServer::SendXorCode(const TcpConnectionPtr &conn) {
    // 发送随机生成的异或码给用户，之后的通信都用该异或码进行加密
    auto gen_val = MessageHeader::GenerateXorCode();
    yy::core::protocol::XorBody xorBody;
    xorBody.set_xor_code(gen_val ^ m_app_configvar->GetValue().app_xor_code()); //! 记得与初始异或码异或

    conn->SetXorCode(gen_val);
    conn->Send(xorBody);
    YLOG_TRACE("Thread_Accepter: 封包异或码<{},{}>给用户<{}>", gen_val,xorBody.xor_code(), conn->GetSocketFD())
}


void WindowsGameServer::AddShutdownConnection(const TcpConnectionPtr & conn) {
    {
        std::lock_guard lg{m_ShutdownConnectionsMutex};
        m_ShutdownConnections.push_back(conn);
    }
    YLOG_TRACE("In TcpServer::AddShutdownConnection<{}>", conn->GetSocketFD());
}

//! 每个IO线程中运行
void WindowsGameServer::CheckDisconnections() {
    YLOG_TRACE("In TcpServer::CheckDisconnections, 有 {} 个shutdown连接", m_ShutdownConnections.size());

    std::vector<TcpConnectionPtr> shutdownConnections;
    {
        std::lock_guard lg{m_ShutdownConnectionsMutex};
        m_ShutdownConnections.swap(shutdownConnections);
    }

    for (const auto & conn: shutdownConnections)
    {
        YLOG_TRACE("In TcpServer::CheckDisconnections, close shutdown socket<{}>", conn->GetSocketFD());

        //! 被Shutdown的用户连接在1秒后正式关闭回收资源
        auto elapsed_time = Timestamp::Now() - conn->GetShudownTime();
        if(conn->IsShutdown())
        {
            if(elapsed_time > Seconds{GetAppConfig().close_delay()}) {
                YLOG_INFO("<{}>主线程Update_CheckDisconnetion: 时辰已到，正式关闭用户连接，回收套接字资源！", conn->GetSocketFD())

                m_users.erase(conn.get());
                conn->Close();

                if(m_notifierDisconnect)
                    m_notifierDisconnect(conn);
            }
        }

        //! ①检查已连接的用户是否在指定时间内通过安全验证，若未通过，则shutdown连接
        elapsed_time = Timestamp::Now() - conn->GetConnectedTime();
        if (conn->IsConnected() and elapsed_time > Seconds{GetAppConfig().time_security_max()})
        {
            YLOG_WARN("<{}>主线程Update_CheckDisconnetion: 用户安全验证超时，关闭用户连接！", conn->GetSocketFD());
            conn->Shutdown();
            return;
        }

        //! ②检查是否收到心跳包，如未收到，则shutdown连接
        elapsed_time = Timestamp::Now() - conn->GetHeartTime();
        if (elapsed_time > Seconds{GetAppConfig().time_heart_max()}) {
            YLOG_WARN("<{}>主线程Update_CheckDisconnetion: 用户心跳包超时，关闭用户连接！", conn->GetSocketFD());
            conn->Shutdown();
            return;
        }
    }
}

void WindowsGameServer::OnHeart(const TcpConnectionPtr & conn, const HeartPtr & message) {
    assert(conn != nullptr);

    // 只需发一个只有消息头的包
    protocol::HeartBody heartBody;
    conn->Send(heartBody);
}

void WindowsGameServer::OnSecurity(const TcpConnectionPtr & conn, const SecurityPtr & message)
{
    assert(conn != nullptr);

    char md5Arr[35]{};
    char Arr[30]{};

    snprintf(Arr, sizeof(Arr), "%s_%d", m_app_configvar->GetValue().security_code(), conn->GetXorCode());
    ::md5::EncryptMD5str(md5Arr, (unsigned char *)(Arr), (int)strlen(Arr));


    YLOG_DEBUG("服务器: {}, {}, {}", GetAppConfig().app_id(), GetAppConfig().app_version(), md5Arr)
    YLOG_DEBUG("客户端: {}, {}, {}", message->app_id(),message->app_version(), message->app_md5().c_str())

    // 进行安全验证，并返回验证结果给用户
    yy::core::protocol::ResultCode resultCode;
    if(message->app_version() != GetAppConfig().app_version()) {
        YLOG_DEBUG("<{}>解包执行：版本不同，安全验证失败！", conn->GetSocketFD())
        resultCode = yy::core::protocol::ResultCode::eAppVersionFailed;
    }
    else if(util::StrCmp_IgnoreCase(message->app_md5().c_str(), md5Arr)) {
        YLOG_DEBUG("<{}>解包执行：md5码不同，安全验证失败！", conn->GetSocketFD())
        resultCode = yy::core::protocol::ResultCode::eMd5Failed;
    }
    else {
        resultCode = yy::core::protocol::ResultCode::eSuccess;
    }
    yy::core::protocol::ResultBody resultBody;
    resultBody.set_result_code(resultCode);
    conn->Send(resultBody);

    // 安全验证通过：交由业务层
    if(resultBody.result_code() == yy::core::protocol::ResultCode::eSuccess) {
        auto baseData = std::make_shared<UserBaseData>(conn, message->app_id());
        baseData->SetState(UserBaseData::E_UserBaseState::eSecure);
        m_users[conn.get()] = baseData;
        m_NumSecurity++;
        if(m_notifierSecurity)
            m_notifierSecurity(conn);
        YLOG_INFO("<%d>解包执行：安全验证通过", conn->GetSocketFD())
    }
    //? 安全验证失败：需要关闭用户连接吗？
    else {
        YLOG_INFO("<%d>解包执行：用户安全验证失败！", conn->GetSocketFD())
    }
}




}