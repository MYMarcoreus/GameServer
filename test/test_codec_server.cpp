#include "TcpServer.h"
#include "TcpConnection.h"
#include "EventLoop.h"
#include "Acceptor.h"
#include "IPAddress.h"
#include "log.h"
#include "ThreadPool.h"
#include "query.pb.h"
#include "codec/ProtobufCodec.h"
#include "codec/ProtobufDispatcher.h"

#include <string>
#include <stdio.h>


using namespace yy;
using namespace yy::net;
using namespace yy::util;
using namespace yy::core;
using std::string;
using yy::protobuf::Query;
using yy::protobuf::Answer;


class QueryServer
{
    using QueryPtr  = std::shared_ptr<yy::protobuf::Query> ;
    using AnswerPtr = std::shared_ptr<yy::protobuf::Answer> ;

public:
    QueryServer(EventLoop* loop, IPAddressPtr listenAddr)
        : loop_{loop},
        server_(loop, listenAddr, true),
        dispatcher_( std::bind(&QueryServer::OnUnknownMessage, this, _1, _2) ),
        codec_(std::bind(&ProtobufDispatcher::OnProtobufMessage, &dispatcher_, _1, _2))
    {
        dispatcher_.RegisterMessageCallback<Query>(std::bind(&QueryServer::OnQuery, this, _1, _2));
        dispatcher_.RegisterMessageCallback<Answer>(std::bind(&QueryServer::OnAnswer, this, _1, _2));
        server_.SetConnectionEstablishedCallback( std::bind(&QueryServer::OnConnectionEstablished, this, _1));
        server_.SetMessageCallback( std::bind(&ProtobufCodec::OnData, &codec_, _1, _2));
    }

    void Start()
    {
        server_.Start(2);      // IO线程
    }

    void SendAnswer(const TcpConnectionPtr& conn, const QueryPtr& query)
    {
        auto now = Timestamp::Now();

        Answer answer;
        answer.set_id(now.GetMircoSecondSinceEpoch().count());
        answer.set_questioner(query->questioner());
        answer.set_answerer("Server");
        answer.add_solution(now.ToString());
        answer.add_solution("Win!");
        YLOG_INFO("即将向<%d: {}>发送Answer：\n{}", conn->GetSocketFD(), conn->GetName().c_str(), answer.DebugString().c_str())
        codec_.Send(conn, answer);
    }

private:
    void OnConnectionEstablished(TcpConnectionPtr conn)
    {
        YLOG_INFO("██████████████████████████████████████████████████████连接成功<%s:%d, %d>！", conn->GetPeerAddr()->GetIPStr().c_str(), conn->GetPeerAddr()->GetPort(), conn->GetSocketFD())
    }

    void OnQuery(const TcpConnectionPtr& conn, const QueryPtr& message)
    {
        YLOG_INFO("OnQuery<%d>: {}\n {}\n", conn->GetSocketFD(), message->GetTypeName().c_str(), message->DebugString().c_str());

        SendAnswer(conn, message);
    }

    void OnAnswer(const TcpConnectionPtr& conn, const AnswerPtr& message)
    {
        string solu{};
        for (int i = 0; i < message->solution_size(); ++i) {
            solu += message->solution(i).c_str();
        }

        YLOG_INFO("OnAnswer: {}, {}, {}, {}, {}", message->GetTypeName().c_str(),
                  message->id(), message->questioner().c_str(), message->answerer().c_str(), solu.c_str())
    }

    void OnUnknownMessage(TcpConnectionPtr conn, const MessagePtr& message)
    {
        YLOG_INFO("未知的消息类型：{}", message->GetDescriptor()->full_name().c_str())
    }


    EventLoop *         loop_;
    TcpServer           server_;
    ProtobufCodec       codec_;
    ProtobufDispatcher  dispatcher_;
};






int main()
{
    config::ConfigManager::LoadConfigs();
    EventLoop loop{true};
    IPAddressPtr listenAddr = std::make_shared<IPv4Address>(config::g_app_config->GetValue().app_port());
    QueryServer server(&loop, listenAddr);
    server.Start();
    loop.Loop();


    return 0;
}
