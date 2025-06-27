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
#include "login.pb.h"

#include "login.pb.h"
#include "LoginService.h"
#include "RpcChannel.h"

using namespace yy;
using namespace yy::net;
using namespace yy::util;
using namespace yy::core;
using namespace yy::protocol::app;
using std::string;

using yy::protocol::core::RpcMessage;

class RpcTestClient
{
    using RpcMessagePtr = std::shared_ptr<RpcMessage> ;
public:
    RpcTestClient(EventLoop * loop, const IPAddressPtr& serverAddr, const bool CanRetry = true) :
        loop_{loop},
        rpc_channel_{std::make_unique<RpcChannel>()},
        stub_{std::make_unique<AccountServiceRpc_Stub>(rpc_channel_.get())},
        client_(loop, serverAddr,
            yy::config::g_remote_config->GetValue().sendBytesOne,
            yy::config::g_remote_config->GetValue().sendBytesMax,
            yy::config::g_remote_config->GetValue().recvBytesOne,
            yy::config::g_remote_config->GetValue().recvBytesMax,
            yy::config::g_remote_config->GetValue().appXorCode
        )
    {
        client_.SetMessageCallback(
            [this](const TcpConnectionPtr& conn,  NetBuffer & buf) {
                rpc_channel_->OnRawMessage(conn, buf);
            });

        client_.SetConnectionEstablishedCallback(
            [this](const TcpConnectionPtr& conn) {
                ConnectionEstablished(conn);
            });

        // client_.SetConnectionWriteCompleteCallback(
        //     [this](const TcpConnectionPtr& conn) {
        //         YLOG_INFO("{}的数据已发送给服务器<{}:{}>", conn->GetLocalAddr()->GetPortStr(), conn->GetPeerAddr()->GetIPStr().c_str(), conn->GetPeerAddr()->GetPort())
        //     });

        // client_.SetCanAutoRetry(CanRetry);
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

        rpc_channel_->SetConnection(conn);

        loop_->RunEvery(100ms, [conn, this]()
        {
            SendLogin(conn);
        });

    }

    void SendLogin(const TcpConnectionPtr& conn)
    {
        C2SLogin request;
        request.set_account_name("nice_client" + std::to_string(cnt_));
        request.set_password("good_pwd" + std::to_string(cnt_));
        request.set_session_id(cnt_);

        cnt_++;

        S2CLogin* response = new S2CLogin;

        stub_->Login(nullptr, &request, response, google::protobuf::NewCallback(this, &RpcTestClient::LoginFinished, response));
    }

    void LoginFinished(S2CLogin* response)
    {
        YLOG_INFO("Login返回结果：{}, {}, {}", response->account_id(), response->account_name(), response->session_id())
        delete response;

        // loop_->QuitLoop();
    }

    yy::net::EventLoop *                                    loop_;
    std::unique_ptr<yy::core::RpcChannel>                   rpc_channel_;
    std::unique_ptr<yy::protocol::app::AccountServiceRpc::Stub>   stub_;
    yy::net::TcpClient                                      client_;

    std::atomic_size_t cnt_;
};





int main()
{
    yy::config::ConfigManager::AddFilePath("../config/configs_gate.xml");
    yy::config::ConfigManager::AddFilePath("../../config/configs_gate.xml");
    yy::config::ConfigManager::LoadXmlConfigs();
    yy::Ylog::LoggerManager::getInstance().ReadConfigs();

    EventLoop loop{500ms};
    IPAddressPtr serverAddr = std::make_shared<IPv4Address>("127.0.0.1", 13334);
    RpcTestClient echoClient{&loop, serverAddr};
    echoClient.Start();
    loop.Loop();


    return 0;
}