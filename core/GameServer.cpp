#include "GameServer.h"
#include "TcpConnection.h"
#include "log.h"
#include "connection.pb.h"
#include "md5/md5.h"
#include "UserBaseData.h"
#include "EventLoop.h"

#include <google/protobuf/message.h>

using namespace yy::net;
using namespace yy::config;

namespace yy::core {

GameServer::GameServer(EventLoop *loop, IPAddressPtr listenAddr)
        : m_accpetorLoop{loop},
          m_server(loop, listenAddr, true),
          m_dispatcher( std::bind(&GameServer::OnUnknownMessage, this, _1, _2) ),
          m_codec(std::bind(&ProtobufDispatcher<TcpConnectionPtr>::OnProtobufMessage, &m_dispatcher, _1, _2)),
          m_app_configvar(g_app_config)
{
    m_dispatcher.RegisterMessageCallback<yy::protocol::core::HeartBody>(std::bind(&GameServer::OnHeart, this, _1, _2));
    m_dispatcher.RegisterMessageCallback<yy::protocol::core::SecurityBody>(std::bind(&GameServer::OnSecurity, this, _1, _2));

    m_server.SetMessageCallback( std::bind(&ProtobufCodec::OnData, &m_codec, _1, _2));
    m_server.SetConnectionEstablishedCallback( std::bind(&GameServer::OnConnectionEstablished, this, _1));
    m_server.SetConnectionShutdownCallback([this](const TcpConnectionPtr & conn) { this->AddShutdownConnection(conn); });
    m_server.SetCloseSocketsCallback([this]() { this->CheckDisconnections(); });
}

GameServer::~GameServer() {
    m_accpetorLoop->QuitLoop();
}



void GameServer::OnUnknownMessage(TcpConnectionPtr conn, const MessagePtr &message) {
    YLOG_TRACE("游戏消息：{}，交由业务层", message->GetDescriptor()->full_name());

    m_notifierCommand(FindUser(conn->GetName()), message);
}




void GameServer::OnConnectionEstablished(TcpConnectionPtr conn) {
    YLOG_INFO("███████████████████连接成功<{}:{}, {}>！",
              conn->GetPeerAddr()->GetIPStr().c_str(), conn->GetPeerAddr()->GetPort(), conn->GetSocketFD());

    SendXorCode(conn);
}

void GameServer::SendXorCode(const TcpConnectionPtr &conn) {
    // 发送随机生成的异或码给用户，之后的通信都用该异或码进行加密
    auto gen_val = MessageHeader::GenerateXorCode();
    yy::protocol::core::XorBody xorBody;
    xorBody.set_xor_code(gen_val ^ GetAppConfig().app_xor_code()); //! 记得与初始异或码异或

    m_codec.Send(conn, xorBody);
    conn->SetXorCode(gen_val); //! 必须在Send后面

    YLOG_TRACE("Thread_Accepter: 封包异或码<{},{}>给用户<{}>，<{},{}>异或标识头<{},{}>", gen_val, xorBody.xor_code(), conn->GetSocketFD(),
               (int)(GetAppConfig().check_code()[0] ), (int)(GetAppConfig().check_code()[1] ),
               (int)(GetAppConfig().check_code()[0] ^ gen_val), (int)(GetAppConfig().check_code()[1] ^ gen_val)
   );
}


void GameServer::AddShutdownConnection(const TcpConnectionPtr & conn) {

    {
        std::lock_guard lg{m_ShutdownConnectionsMutex};
        m_ShutdownConnections.push_back(conn);
    }
    YLOG_TRACE("In TcpServer::AddShutdownConnection<{}>", conn->GetSocketFD());
    // YLOG_INFO("{} ?= {}", static_cast<void*>(m_accpetorLoop), static_cast<void*>(m_server.GetAcceptorLoop()));
    //FIXME 潜在的线程安全问题
    m_accpetorLoop->Wakeup();
}

//! 每个IO线程中运行
void GameServer::CheckDisconnections() {
    YLOG_TRACE("In TcpServer::CheckDisconnections, 有 {} 个shutdown连接", m_ShutdownConnections.size());

    std::vector<TcpConnectionPtr> shutdownConnections;
    {
        std::lock_guard lg{m_ShutdownConnectionsMutex};
        m_ShutdownConnections.swap(shutdownConnections);
    }

    for (const auto & conn: shutdownConnections)
    {
        // YLOG_DEBUG("In TcpServer::CheckDisconnections, 有 {} 个shutdown连接", m_ShutdownConnections.size());
        YLOG_DEBUG("In TcpServer::CheckDisconnections, close shutdown socket<{}>", conn->GetSocketFD());

        //! 被Shutdown的用户连接在1秒后正式关闭回收资源
        auto elapsed_time = Timestamp::Now() - conn->GetShudownTime();
        if(conn->IsShutdown())
        {
            if(elapsed_time > Seconds{GetAppConfig().close_delay()}) {
                YLOG_INFO("<{}>主线程Update_CheckDisconnetion: 时辰已到，正式关闭用户连接，回收套接字资源！", conn->GetSocketFD())

                //! 应用层处理
                if(m_notifierDisconnect)
                    m_notifierDisconnect(conn);

                //! 核心层处理
                m_users.erase(conn->GetName());
                conn->Close();

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

void GameServer::OnHeart(const TcpConnectionPtr & conn, const HeartPtr & message) {
    assert(conn != nullptr);

    // 只需发一个只有消息头的包
    yy::protocol::core::HeartBody heartBody;
    m_codec.Send(conn, heartBody);
}

void GameServer::OnSecurity(const TcpConnectionPtr & conn, const SecurityPtr & message)
{
    assert(conn != nullptr);

    char md5Arr[35]{};
    char Arr[30]{};

    snprintf(Arr, sizeof(Arr), "%s_%d", m_app_configvar->GetValue().security_code(), conn->GetXorCode());
    ::md5::EncryptMD5str(md5Arr, (unsigned char *)(Arr), (int)strlen(Arr));


    YLOG_DEBUG("服务器: {}, {}, {}", GetAppConfig().app_id(), GetAppConfig().app_version(), md5Arr)
    YLOG_DEBUG("客户端: {}, {}, {}", message->app_id(),message->app_version(), message->app_md5().c_str())

    // 进行安全验证，并返回验证结果给用户
    yy::protocol::core::ResultCode resultCode;
    if(message->app_version() != GetAppConfig().app_version()) {
        YLOG_DEBUG("<{}>解包执行：版本不同，安全验证失败！", conn->GetSocketFD())
        resultCode = yy::protocol::core::ResultCode::eAppVersionFailed;
    }
    else if(util::StrCmp_IgnoreCase(message->app_md5().c_str(), md5Arr)) {
        YLOG_DEBUG("<{}>解包执行：md5码不同，安全验证失败！", conn->GetSocketFD())
        resultCode = yy::protocol::core::ResultCode::eMd5Failed;
    }
    else {
        resultCode = yy::protocol::core::ResultCode::eSuccess;
    }
    yy::protocol::core::ResultBody resultBody;
    resultBody.set_result_code(resultCode);
    m_codec.Send(conn, resultBody);

    // 安全验证通过：交由业务层
    if(resultBody.result_code() == yy::protocol::core::ResultCode::eSuccess) {
        auto userdata = std::make_shared<UserBaseData>(conn, message->app_id(), m_codec);
        userdata->SetState(UserBaseData::E_UserBaseState::eSecure);
        m_users[conn->GetName()] = userdata;
        m_NumSecurity++;
        if(m_notifierSecurity)
            m_notifierSecurity(conn);
        YLOG_INFO("<{}>解包执行：安全验证通过", conn->GetSocketFD())
    }
    //? 安全验证失败：需要关闭用户连接吗？
    else {
        YLOG_INFO("<{}>解包执行：用户安全验证失败！", conn->GetSocketFD())
    }
}

UserBaseDataPtr GameServer::FindUser(const std::string & conn) {
    auto it = m_users.find(conn);
    return (it == m_users.end()) ? nullptr : it->second;
}

void GameServer::SetUserFree(const UserBaseDataPtr &userdata) {
    userdata->Shutdown();
    m_users.erase(userdata->GetConnection()->GetName());
}

void GameServer::Start() {
    m_server.Start(config::g_app_config->GetValue().io_thread_num());
    m_accpetorLoop->Loop();
}




net::TimerID GameServer::RunAt(net::Timestamp time, net::F_TimerCallback cb) {
    return m_accpetorLoop->RunAt(time, std::move(cb));
}

net::TimerID GameServer::RunAfter(net::Microseconds delay, net::F_TimerCallback cb) {
    return m_accpetorLoop->RunAfter(delay, std::move(cb));
}

net::TimerID GameServer::RunEvery(net::Microseconds interval, net::F_TimerCallback cb) {
    return m_accpetorLoop->RunEvery(interval, std::move(cb));
}

void GameServer::CancelTimer(net::TimerID timerid) {
    m_accpetorLoop->CancelTimer(timerid);
}





}