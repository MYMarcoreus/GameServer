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

    void SendAnswer(const TcpConnectionPtr& conn, const QueryPtr& query)
    {
        const auto now = Timestamp::Now();

        Answer answer;
        answer.set_id(now.GetMircoSecondSinceEpoch().count());
        answer.set_questioner(query->questioner());
        answer.set_answerer("Server");
        answer.add_solution(now.ToString());
        answer.add_solution("Win!");
        YLOG_INFO("即将向<{}:{}>发送Answer：\n{}", conn->GetSocketFD(), conn->GetConnID(), answer.DebugString().c_str())
        codec_.SendTCP(conn, answer);
    }

private:
    void OnConnectionEstablished(const TcpConnectionPtr& conn)
    {
        YLOG_INFO("██████████████████████████████████████████████████████连接成功<%s:%d, %d>！", conn->GetPeerAddr()->GetIPStr().c_str(), conn->GetPeerAddr()->GetPort(), conn->GetSocketFD())
    }

    void OnQuery(const TcpConnectionPtr& conn, const QueryPtr& message)
    {
        static std::atomic<size_t> cnt_ = 0;

        string ques{};
        for (int i = 0; i < message->question_size(); ++i) {
            ques += message->question(i).c_str();
        }
        cnt_.fetch_add(1, std::memory_order_relaxed);

        // YLOG_INFO("收到用户的Query<{}>: \nid:{} \nquestioner: {} \nquestion: {}", cnt_,
        //     message->id(),
        //     message->questioner(),
        //     ques);

        YLOG_INFO("收到用户的Query<{}>: id:{}; questioner: {}; question: {};", cnt_.load(), message->id(), message->questioner(), ques);

        SendAnswer(conn, message);
    }

    void OnUnknownMessage(TcpConnectionPtr conn, const MessagePtr& message)
    {
        YLOG_INFO("未知的消息类型：{}", message->GetDescriptor()->full_name().c_str())
    }


    EventLoop *                             loop_;
    TcpServer                               server_;
    ProtobufDispatcher<TcpConnectionPtr>    dispatcher_;
    ProtobufTcpCodec                        codec_;
};





int main()
{
    yy::config::ConfigManager::AddFilePath("../config/configs_logic.xml");
    yy::config::ConfigManager::AddFilePath("../../config/configs_logic.xml");
    yy::config::ConfigManager::LoadXmlConfigs();
    yy::Ylog::LoggerManager::Instance().ReadConfigs();

    EventLoop loop{500ms};
    const IPAddressPtr listenAddr = std::make_shared<IPv4Address>(config::g_app_config->GetValue().app_tcp_port());
    QueryServer server(&loop, listenAddr);
    server.Start();
    loop.Loop();


    return 0;
}
