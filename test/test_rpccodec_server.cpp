#include "TcpServer.h"
#include "TcpConnection.h"
#include "EventLoop.h"
#include "Acceptor.h"
#include "IPAddress.h"
#include "log.h"
#include "ThreadPool.h"
#include "RpcCodec.h"
#include "ProtobufDispatcher.h"
#include "rpc.pb.h"

#include <string>
#include <stdio.h>


using namespace yy;
using namespace yy::net;
using namespace yy::util;
using namespace yy::core;
using std::string;
using yy::protocol::core::RpcMessage;


class QueryServer
{
    using RpcMessagePtr = std::shared_ptr<RpcMessage> ;

public:
    QueryServer(EventLoop* loop, const IPAddressPtr& listenAddr) :
    loop_{loop},
    server_(loop, listenAddr, true,
            yy::config::g_app_config->GetValue().send_bytes_one(),
            yy::config::g_app_config->GetValue().send_bytes_max(),
            yy::config::g_app_config->GetValue().recv_bytes_one(),
            yy::config::g_app_config->GetValue().recv_bytes_max(),
            yy::config::g_app_config->GetValue().app_xor_code()
    ),
    dispatcher_([this](const TcpConnectionPtr& conn, const MessagePtr& msg) {
        OnUnknownMessage(conn, msg);
    }),
    codec_([this](const TcpConnectionPtr& conn, const MessagePtr& buf) {
        dispatcher_.OnProtobufMessage(conn, buf);
    })
    {
        dispatcher_.RegisterMessageCallback<RpcMessage>(
            [this](const TcpConnectionPtr& conn, const std::shared_ptr<RpcMessage>& msg) {
                OnQuery(conn, msg);
            });

        server_.SetConnectionEstablishedCallback(
            [this](const TcpConnectionPtr& conn) {
                OnConnectionEstablished(conn);
            });

        server_.SetMessageCallback(
            [this](const TcpConnectionPtr &conn, NetBuffer &buf) {
                codec_.OnTcpData(conn, buf);
            });
    }


    void Start()
    {
        server_.Start(2, 500ms);      // IO线程

        loop_->RunEvery(1000ms, [this]()
        {
            YLOG_INFO("服务器定时器");
        });
    }

private:
    void OnConnectionEstablished(const TcpConnectionPtr& conn)
    {
        YLOG_INFO("██████████████████████████████████████████████████████连接成功<%s:%d, %d>！", conn->GetPeerAddr()->GetIPStr().c_str(), conn->GetPeerAddr()->GetPort(), conn->GetSocketFD())
    }

    void OnQuery(const TcpConnectionPtr& conn, const RpcMessagePtr& message)
    {
        static std::atomic_size_t cnt_ = 0;
        cnt_.fetch_add(1, std::memory_order_relaxed);

        YLOG_INFO("OnQuery<{}>: {}; {};", cnt_.load(), static_cast<int>(message->type()), message->id())

        RpcMessage msg;
        msg.set_type(protocol::core::RpcMessage_Type_RESPONSE);
        msg.set_id(20001);
        codec_.SendTCP(conn, msg);

        conn->Shutdown();
    }


    void OnUnknownMessage(TcpConnectionPtr conn, const MessagePtr& message)
    {
        YLOG_INFO("未知的消息类型：{}", message->GetDescriptor()->full_name().c_str())
    }


    EventLoop *                             loop_;
    TcpServer                               server_;
    ProtobufDispatcher<TcpConnectionPtr>    dispatcher_;
    RpcCodec                                codec_;
};






int main()
{
    yy::config::ConfigManager::AddFilePath("../config/configs_login.xml");
    yy::config::ConfigManager::AddFilePath("../../config/configs_login.xml");
    yy::config::ConfigManager::LoadXmlConfigs();
    yy::Ylog::LoggerManager::Instance().ReadConfigs();

    EventLoop loop{500ms};
    const IPAddressPtr listenAddr = std::make_shared<IPv4Address>(config::g_app_config->GetValue().app_tcp_port());
    QueryServer server(&loop, listenAddr);
    server.Start();
    loop.Loop();


    return 0;
}

