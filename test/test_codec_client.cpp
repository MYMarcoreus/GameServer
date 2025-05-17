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
    QueryClient(EventLoop * loop, IPAddressPtr serverAddr, bool CanRetry = true)
        : client_(loop, serverAddr),
          loop_{loop},
          dispatcher_( std::bind(&QueryClient::OnUnknownMessage, this, _1, _2) ),
          codec_(std::bind(&decltype(dispatcher_)::OnProtobufMessage, &dispatcher_, _1, _2))
    {
        dispatcher_.RegisterMessageCallback<Empty>(std::bind(&QueryClient::OnEmpty, this, _1, _2));
        dispatcher_.RegisterMessageCallback<Answer>(std::bind(&QueryClient::OnAnswer, this, _1, _2));
        client_.SetMessageCallback(std::bind(&ProtobufTcpCodec::OnData, &codec_, _1, _2));
        client_.SetConnectionEstablishedCallback(std::bind(&QueryClient::ConnectionEstablished, this, _1));
        client_.SetConnectionWriteCompleteCallback(std::bind(&QueryClient::ConnectionWriteComplete, this, _1));
        client_.SetCanAutoRetry(CanRetry);
    }

    void Start()
    {
        client_.Connect();
    }

    void Send(std::string message)
    {
        client_.GetConnection()->SendTCP(message);
    }

private:
    void ConnectionEstablished(TcpConnectionPtr conn) {
        YLOG_INFO("连接至<{}:{}>，我方地址为<{}:{}>", conn->GetPeerAddr()->GetIPStr().c_str(), conn->GetPeerAddr()->GetPort()
                                                 , conn->GetLocalAddr()->GetIPStr().c_str(), conn->GetLocalAddr()->GetPort());

        SendQuery(conn);
        loop_->RunAfter(500ms, [conn, this]() { SendQuery(conn); });
    }

    void ConnectionWriteComplete(TcpConnectionPtr conn) {
        YLOG_INFO("数据已发送给服务器<{}:{}>", conn->GetPeerAddr()->GetIPStr().c_str(), conn->GetPeerAddr()->GetPort())
    }

    void SendQuery(TcpConnectionPtr conn)
    {
        Query query;
        query.set_id(Timestamp::Now().GetMircoSecondSinceEpoch().count());
        query.set_questioner("Client");
        query.add_question("What time?");
        // Empty empty;
        google::protobuf::Message* messageToSend = &query;
        YLOG_INFO("即将向<{}: {}>发送Query：\n{}", conn->GetSocketFD(), conn->GetName().c_str(), query.DebugString().c_str())
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
        // loop_->QuitLoop();
        loop_->RunAfter(100ms, [l = loop_](){ l->QuitLoop();} );
    }

    EventLoop *         loop_;
    TcpClient           client_;
    ProtobufTcpCodec       codec_;
    ProtobufDispatcher<TcpConnectionPtr>  dispatcher_;
};





int main()
{
    config::ConfigManager::LoadXmlConfigs();
    EventLoop loop{500ms};
    auto serverNode = config::g_remote_config->GetValue().m_remote_nodes[0];
    IPAddressPtr serverAddr = std::make_shared<IPv4Address>(serverNode.m_ip, serverNode.m_port);
    QueryClient echoClient{&loop, serverAddr};
    echoClient.Start();
    loop.Loop();


    return 0;
}