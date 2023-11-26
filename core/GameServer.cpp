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
          m_codec(std::bind(&decltype(m_dispatcher)::OnProtobufMessage, &m_dispatcher, _1, _2)),
          m_app_configvar(g_app_config)
{
    m_dispatcher.RegisterMessageCallback<yy::protocol::core::HeartBody>(std::bind(&GameServer::OnHeart, this, _1, _2));
    m_dispatcher.RegisterMessageCallback<yy::protocol::core::SecurityBody>(std::bind(&GameServer::OnSecurity, this, _1, _2));

    m_server.SetMessageCallback( std::bind(&ProtobufCodec::OnData, &m_codec, _1, _2));
    m_server.SetConnectionEstablishedCallback( std::bind(&GameServer::OnConnectionEstablished, this, _1));
    m_server.SetConnectionShutdownCallback([this](const TcpConnectionPtr & conn) { this->AfterShutdownConnection(conn); });
    // m_server.SetCloseSocketsCallback([this]() { this->CheckDisconnections(); });
}

GameServer::~GameServer() {
    Stop();
}

void GameServer::Start() {
    m_server.Start(config::g_app_config->GetValue().io_thread_num(), 500ms);

    // m_accpetorLoop->RunEvery(1s, [](){ YLOG_INFO("测试！！！"); });
}

void GameServer::Stop() {
    m_accpetorLoop->QuitLoop();
}


void GameServer::OnUnknownMessage(const TcpConnectionPtr & conn, const MessagePtr &message) {
    YLOG_TRACE("游戏消息：{}，交由业务层", message->GetDescriptor()->full_name());

    m_notifier_command(FindUser(conn->GetName()), message);
}

void GameServer::OnConnectionEstablished(const TcpConnectionPtr  & conn) {
    YLOG_INFO("███████████████████连接成功<{}:{}, {}>！",
              conn->GetPeerAddr()->GetIPStr().c_str(), conn->GetPeerAddr()->GetPort(), conn->GetSocketFD());

    auto userdata = std::make_shared<UserBaseData>(conn, m_codec);
    AddUser(conn->GetName(), userdata);
    AddCheckTimer(conn, userdata);
    SendXorCode(conn);
}

void GameServer::AddCheckTimer(const TcpConnectionPtr & conn, const UserBaseDataPtr & userdata) {
    /* ***** 需要是弱引用，不能因为这个回调函数延长TcpConnection的生命周期 ***** */
    //! ①检查是否在指定时间内完成安全连接的认证，若未认证，则关闭连接
    conn->GetLoop()->RunAfter(Seconds{GetAppConfig().time_security_max()},
    [weak_userdata = std::weak_ptr<UserBaseData>{userdata}]()
    {
        if(auto userdata = weak_userdata.lock()) {
            if (userdata->IsConnected() and !userdata->IsSecure()) {
                YLOG_WARN("<{},{}>主线程Update_CheckDisconnetion: 用户安全验证超时，关闭用户连接！", userdata->GetSocketFD(), userdata->GetConnName());
                userdata->Shutdown();
            }
        }
    });

    //! ②检查是否收到心跳包，如未收到，则shutdown连接
    conn->GetLoop()->RunAfter(Seconds{GetAppConfig().time_heart_max()},
    [this, weak_userdata = std::weak_ptr<UserBaseData>{userdata}]()
    {
        if(auto userdata = weak_userdata.lock()) {
            this->CheckHeart(userdata);
        }
    });
}

void GameServer::CheckHeart(const UserBaseDataPtr & userdata) {
    const auto & conn = userdata->GetConnection();
    if(!conn->IsConnected() or Timestamp::Now() - conn->GetHeartTime() > Seconds{g_app_config->GetValue().time_heart_max()}) {
        YLOG_WARN("<{}>主线程Update_CheckDisconnetion: 用户心跳包超时，关闭用户连接！", conn->GetSocketFD());
        conn->Shutdown();
        // conn->GetLoop()->CancelTimer(this->m_heartTimerID); //! 不生效因为执行该函数时Timer不在列表中，执行完才加入列表
    } else {
        //! 需要是弱引用，不能因为这个回调函数延长TcpConnection的生命周期
        conn->GetLoop()->RunAfter(Seconds{GetAppConfig().time_heart_max()},
        [this, weak_userdata = std::weak_ptr<UserBaseData>{userdata}]
        {
          if(auto userdata = weak_userdata.lock()) {
              this->CheckHeart(userdata);
          }
        });
    }
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
        m_num_security++;
        if(m_notifier_security)
            m_notifier_security(conn);
        YLOG_INFO("<{}>解包执行：安全验证通过", conn->GetSocketFD())
    }
    //? 安全验证失败：需要关闭用户连接吗？
    else {
        YLOG_INFO("<{}>解包执行：用户安全验证失败！", conn->GetSocketFD())
    }
}







net::TimerID GameServer::RunAt(net::Timestamp time, net::F_TaskCallback cb) {
    return m_accpetorLoop->RunAt(time, std::move(cb));
}

net::TimerID GameServer::RunAfter(net::Microseconds delay, net::F_TaskCallback cb) {
    return m_accpetorLoop->RunAfter(delay, std::move(cb));
}

net::TimerID GameServer::RunEvery(net::Microseconds interval, net::F_TaskCallback cb) {
    return m_accpetorLoop->RunEvery(interval, std::move(cb));
}

void GameServer::CancelTimer(net::TimerID timerid) {
    m_accpetorLoop->CancelTimer(timerid);
}



void GameServer::AfterShutdownConnection(const TcpConnectionPtr & conn) {
    //! 应用层处理
    if(m_notifier_disconnect)
        m_notifier_disconnect(conn);
}



UserBaseDataPtr GameServer::FindUser(const std::string & conn_name) {
    std::lock_guard lg{m_users_mutex};

    auto it = m_users.find(conn_name);
    return (it == m_users.end()) ? nullptr : it->second;
}

void GameServer::DelUser(const std::string & conn_name) {
    std::lock_guard lg{m_users_mutex};

    m_users.erase(conn_name);
}

void GameServer::AddUser(const std::string & conn_name, const UserBaseDataPtr & userdata) {
    std::lock_guard lg{m_users_mutex};

    m_users[conn_name] = userdata;
}






}
