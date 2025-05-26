#include "TcpClient.h"
#include "TcpConnection.h"
#include "EventLoop.h"
#include "IPAddress.h"
#include "log.h"
#include "codec/ProtobufTcpCodec.h"
#include "codec/ProtobufDispatcher.h"
#include "query.pb.h"
#include "RemoteXmlConfig.h"
#include <stdio.h>

using namespace yy;
using namespace yy::net;
using namespace yy::util;
using namespace yy::core;
using std::string;
using yy::protobuf::Query;
using yy::protobuf::Answer;
using yy::protobuf::Empty;



class QueryClient
{
    using QueryPtr  = std::shared_ptr<yy::protobuf::Query> ;
    using AnswerPtr = std::shared_ptr<yy::protobuf::Answer> ;
    using EmptyPtr = std::shared_ptr<yy::protobuf::Empty> ;
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
        dispatcher_.RegisterMessageCallback<Empty>(
            [this](const TcpConnectionPtr& conn, const EmptyPtr& msg) {
                OnEmpty(conn, msg);
            });

        dispatcher_.RegisterMessageCallback<Answer>(
            [this](const TcpConnectionPtr& conn, const AnswerPtr& msg) {
                OnAnswer(conn, msg);
            });

        client_.SetMessageCallback(
            [this](const TcpConnectionPtr& conn,  Buffer & buf) {
                codec_.OnData(conn, buf);
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

        // SendQuery(conn);
        loop_->RunEvery(1000ms, [conn, this]()
        {
            SendQuery(conn);
        });
    }

    void ConnectionWriteComplete(const TcpConnectionPtr& conn) {
        YLOG_INFO("数据已发送给服务器<{}:{}>", conn->GetPeerAddr()->GetIPStr().c_str(), conn->GetPeerAddr()->GetPort())
    }

    void SendQuery(const TcpConnectionPtr& conn)
    {
        Query query;
        query.set_id(Timestamp::Now().GetMircoSecondSinceEpoch().count());
        query.set_questioner("Client");
        query.add_question("What time?");
        // Empty empty;
        const google::protobuf::Message* messageToSend = &query;
        YLOG_INFO("即将向<{}: {}>发送Query[{} Byte]：\n{}", conn->GetSocketFD(), conn->GetName().c_str(), query.ByteSizeLong(), query.DebugString().c_str());
        codec_.SendTCP(conn, *messageToSend);
    }

    void OnUnknownMessage(TcpConnectionPtr conn, const MessagePtr& message)
    {
        YLOG_INFO("未知的消息类型：{}", message->GetDescriptor()->full_name().c_str())
    }

    void OnEmpty(const TcpConnectionPtr& conn, const EmptyPtr & message)
    {
        YLOG_INFO("OnEmpty: {}\n{}\n",   message->GetTypeName().c_str(), message->DebugString().c_str());
    }

    void OnAnswer(const TcpConnectionPtr& conn, const AnswerPtr& message)
    {
        string solu{};
        for (int i = 0; i < message->solution_size(); ++i) {
            solu += message->solution(i).c_str();
        }

        YLOG_INFO("OnAnswer: {}\n{}\n", message->GetTypeName(), message->DebugString())
        // loop_->RunAfter(100ms, [l = loop_](){ l->QuitLoop();} );
    }

    EventLoop *                             loop_;
    TcpClient                               client_;
    ProtobufDispatcher<TcpConnectionPtr>    dispatcher_;
    ProtobufTcpCodec                        codec_;
};





int main()
{
    config::ConfigManager::AddFilePath("./configs_client.xml");
    config::ConfigManager::AddFilePath("../configs_client.xml");
    yy::config::ConfigManager::LoadXmlConfigs();
    yy::Ylog::LoggerManager::getInstance().ReadConfigs();

    EventLoop loop{500ms};
    auto serverNode = config::g_remote_config->GetValue().m_remote_nodes[0];
    IPAddressPtr serverAddr = std::make_shared<IPv4Address>(serverNode.m_ip, serverNode.m_port);
    QueryClient echoClient{&loop, serverAddr};
    echoClient.Start();
    loop.Loop();


    return 0;
}