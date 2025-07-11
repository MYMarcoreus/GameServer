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
        client_(loop,
            yy::config::g_remote_config->GetValue().sendBytesOne,
            yy::config::g_remote_config->GetValue().sendBytesMax,
            yy::config::g_remote_config->GetValue().recvBytesOne,
            yy::config::g_remote_config->GetValue().recvBytesMax,
            yy::config::g_remote_config->GetValue().appXorCode,
            serverAddr
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
        client_.GetConnection()->SendRawTCP(message);
    }

private:
    void ConnectionEstablished(const TcpConnectionPtr& conn) {
        YLOG_INFO("连接至<{}:{}>，我方地址为<{}:{}>", conn->GetPeerAddr()->GetIPStr().c_str(), conn->GetPeerAddr()->GetPort()
                                                 , conn->GetLocalAddr()->GetIPStr().c_str(), conn->GetLocalAddr()->GetPort());

        loop_->RunEvery(10ms, [weak_conn = std::weak_ptr{conn}, this]()
        {
            if (const auto shared_conn = weak_conn.lock()) {
                SendQuery(shared_conn);
            }
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
        // YLOG_INFO("即将向<{}: {}>发送Query[{} Byte]：\n{}", conn->GetSocketFD(), conn->GetConnID(), query.ByteSizeLong(), query.DebugString().c_str());
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
        static std::atomic_size_t cnt_ = 0;

        string solu{};
        for (int i = 0; i < message->solution_size(); ++i) {
            solu += message->solution(i).c_str();
        }
        cnt_.fetch_add(1, std::memory_order_relaxed);
        YLOG_INFO("OnAnswer<{}>: {}; {};", cnt_.load(), message->GetTypeName(), message->DebugString())
    }

    EventLoop *                             loop_;
    TcpClient                               client_;
    ProtobufDispatcher<TcpConnectionPtr>    dispatcher_;
    ProtobufTcpCodec                        codec_;
};





int main()
{
    yy::config::ConfigManager::AddFilePath("../config/configs_client.xml");
    yy::config::ConfigManager::AddFilePath("../../config/configs_client.xml");
    yy::config::ConfigManager::LoadXmlConfigs();

    START_YLOG_AFTER_CONFIG()

    yy::net::EventLoop loop{200ms};
    auto serverNode = config::g_remote_config->GetValue().m_remote_nodes[0];
    IPAddressPtr serverAddr = std::make_shared<IPv4Address>(serverNode.ip, serverNode.port);

    std::vector<std::unique_ptr<QueryClient>> clients;
    for (int i = 0; i < 20; ++i) {
        auto client = std::make_unique<QueryClient>(&loop, serverAddr);
        client->Start();
        clients.emplace_back(std::move(client));
    }

    loop.Loop();

    CLOSE_YLOG();
    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}