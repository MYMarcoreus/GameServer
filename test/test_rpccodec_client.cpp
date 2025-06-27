#include "TcpClient.h"
#include "TcpConnection.h"
#include "EventLoop.h"
#include "IPAddress.h"
#include "log.h"
#include "RpcCodec.h"
#include "codec/ProtobufDispatcher.h"
#include "rpc.pb.h"
#include "RemoteXmlConfig.h"
#include <stdio.h>

using namespace yy;
using namespace yy::net;
using namespace yy::util;
using namespace yy::core;
using std::string;

using yy::protocol::core::RpcMessage;

class QueryClient
{
    using RpcMessagePtr = std::shared_ptr<RpcMessage> ;
public:
    QueryClient(EventLoop * loop, const IPAddressPtr& serverAddr, const bool CanRetry = true) :
        loop_{loop},
        client_(loop, serverAddr,
            yy::config::g_remote_config->GetValue().sendBytesOne,
            yy::config::g_remote_config->GetValue().sendBytesMax,
            yy::config::g_remote_config->GetValue().recvBytesOne,
            yy::config::g_remote_config->GetValue().recvBytesMax,
            yy::config::g_remote_config->GetValue().appXorCode
        ),
          dispatcher_([this](const TcpConnectionPtr& conn, const MessagePtr& msg) {
              OnUnknownMessage(conn, msg);
          }),
          codec_([this](const TcpConnectionPtr& conn, const MessagePtr & buf) {
              dispatcher_.OnProtobufMessage(conn, buf);
          })
    {
        client_.SetMessageCallback(
            [this](const TcpConnectionPtr& conn,  NetBuffer & buf) {
                codec_.OnTcpData(conn, buf);
            });

        client_.SetConnectionEstablishedCallback(
            [this](const TcpConnectionPtr& conn) {
                ConnectionEstablished(conn);
            });

        client_.SetConnectionWriteCompleteCallback(
            [this](const TcpConnectionPtr& conn) {
                ConnectionWriteComplete(conn);
            });

        client_.SetCanAutoRetry(CanRetry);
    }


    void Start()
    {
        client_.Connect();
    }

    void Send(const std::string& message)
    {
        client_.GetConnection()->SendTCP(message);
    }

private:
    void ConnectionEstablished(const TcpConnectionPtr& conn) {
        YLOG_INFO("连接至<{}:{}>，我方地址为<{}:{}>", conn->GetPeerAddr()->GetIPStr().c_str(), conn->GetPeerAddr()->GetPort()
                                                 , conn->GetLocalAddr()->GetIPStr().c_str(), conn->GetLocalAddr()->GetPort());

        loop_->RunEvery(100ms, [conn, this]()
        {
            SendQuery(conn);
        });
    }

    void ConnectionWriteComplete(const TcpConnectionPtr& conn) {
        YLOG_INFO("数据已发送给服务器<{}:{}>", conn->GetPeerAddr()->GetIPStr().c_str(), conn->GetPeerAddr()->GetPort())
    }

    void SendQuery(const TcpConnectionPtr& conn)
    {
        RpcMessage msg;
        msg.set_type(protocol::core::RpcMessage_Type_REQUEST);
        msg.set_id(10086);

        YLOG_INFO("即将向<{}: {}>发送Query[{} Byte]：\n{}\n{}\n{}", conn->GetSocketFD(), conn->GetConnID(), msg.ByteSizeLong(),
            msg.DebugString().c_str(),
            msg.id(),
            (int)msg.type());
        codec_.SendTCP(conn, msg);
    }

    void OnUnknownMessage(TcpConnectionPtr conn, const MessagePtr& message)
    {
        YLOG_INFO("未知的消息类型：{}", message->GetDescriptor()->full_name().c_str())
    }

    EventLoop *                             loop_;
    TcpClient                               client_;
    ProtobufDispatcher<TcpConnectionPtr>    dispatcher_;
    RpcCodec                        codec_;
};





int main()
{
    yy::config::ConfigManager::AddFilePath("../config/configs_gate.xml");
    yy::config::ConfigManager::AddFilePath("../../config/configs_gate.xml");
    yy::config::ConfigManager::LoadXmlConfigs();
    yy::Ylog::LoggerManager::getInstance().ReadConfigs();

    EventLoop loop{500ms};
    IPAddressPtr serverAddr = std::make_shared<IPv4Address>("127.0.0.1", 13334);
    QueryClient echoClient{&loop, serverAddr};
    echoClient.Start();
    loop.Loop();


    return 0;
}