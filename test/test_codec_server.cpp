#include "TcpServer.h"
#include "TcpConnection.h"
#include "EventLoop.h"
#include "Acceptor.h"
#include "IPAddress.h"
#include "log.h"
#include "ThreadPool.h"
#include "query.pb.h"
#include "codec/ProtobufTcpCodec.h"
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
        dispatcher_.RegisterMessageCallback<Query>(
            [this](const TcpConnectionPtr& conn, const std::shared_ptr<Query>& msg) {
                OnQuery(conn, msg);
            });

        dispatcher_.RegisterMessageCallback<Answer>(
            [this](const TcpConnectionPtr& conn, const std::shared_ptr<Answer>& msg) {
                OnAnswer(conn, msg);
            });

        server_.SetConnectionEstablishedCallback(
            [this](const TcpConnectionPtr& conn) {
                OnConnectionEstablished(conn);
            });

        server_.SetMessageCallback(
            [this](const TcpConnectionPtr &conn, Buffer &buf) {
                codec_.OnData(conn, buf);
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

    void SendAnswer(const TcpConnectionPtr& conn, const QueryPtr& query)
    {
        auto now = Timestamp::Now();

        Answer answer;
        answer.set_id(now.GetMircoSecondSinceEpoch().count());
        answer.set_questioner(query->questioner());
        answer.set_answerer("Server");
        answer.add_solution(now.ToString());
        answer.add_solution("Win!");
        YLOG_INFO("即将向<{}:{}>发送Answer：\n{}", conn->GetSocketFD(), conn->GetName().c_str(), answer.DebugString().c_str())
        codec_.SendTCP(conn, answer);
    }

private:
    void OnConnectionEstablished(const TcpConnectionPtr& conn)
    {
        YLOG_INFO("██████████████████████████████████████████████████████连接成功<%s:%d, %d>！", conn->GetPeerAddr()->GetIPStr().c_str(), conn->GetPeerAddr()->GetPort(), conn->GetSocketFD())
    }

    void OnQuery(const TcpConnectionPtr& conn, const QueryPtr& message)
    {
        string ques{};
        for (int i = 0; i < message->question_size(); ++i) {
            ques += message->question(i).c_str();
        }

        YLOG_INFO("收到用户的Query<{}>: \nid:{} \nquestioner: {} \nquestion: {}", conn->GetSocketFD(), message->id(), message->questioner(), ques);

        SendAnswer(conn, message);
    }

    void OnAnswer(const TcpConnectionPtr& conn, const AnswerPtr& message)
    {
        string solu{};
        for (int i = 0; i < message->solution_size(); ++i) {
            solu += message->solution(i).c_str();
        }

        YLOG_INFO("OnAnswer: \n{}, \n{}, \n{}, \n{}, \n{}", message->GetTypeName().c_str(),
                  message->id(), message->questioner().c_str(), message->answerer().c_str(), solu.c_str())
    }

    void OnUnknownMessage(TcpConnectionPtr conn, const MessagePtr& message)
    {
        YLOG_INFO("未知的消息类型：{}", message->GetDescriptor()->full_name().c_str())
    }


    EventLoop *         loop_;
    TcpServer           server_;
    ProtobufDispatcher<TcpConnectionPtr>  dispatcher_;
    ProtobufTcpCodec       codec_;
};






int main()
{

    config::ConfigManager::AddFilePath("./configs.xml");
    config::ConfigManager::AddFilePath("../configs.xml");
    yy::config::ConfigManager::LoadXmlConfigs();
    yy::Ylog::LoggerManager::getInstance().ReadConfigs();

    EventLoop loop{500ms};
    const IPAddressPtr listenAddr = std::make_shared<IPv4Address>(config::g_app_config->GetValue().app_tcp_port());
    QueryServer server(&loop, listenAddr);
    server.Start();
    loop.Loop();


    return 0;
}
