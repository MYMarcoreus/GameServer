#include "TcpClient.h"
#include "TcpConnection.h"
#include "EventLoop.h"
#include "IPAddress.h"
#include "log.h"
#include "Buffer.h"
#include <stdio.h>

using namespace yy;
using namespace yy::net;
using namespace yy::util;
using std::string;



class EchoClient
{
public:
    EchoClient(EventLoop * loop, const IPAddressPtr& serverAddr, const bool CanRetry = true)
        : m_Client(loop, serverAddr)
    {
        m_Client.SetMessageCallback([this](const TcpConnectionPtr & conn, Buffer & buf){ this->OnRecvMessage(conn, buf); });
        m_Client.SetConnectionEstablishedCallback([this](const TcpConnectionPtr& conn){ this->ConnectionEstablished(conn); });
        m_Client.SetConnectionWriteCompleteCallback([this](const TcpConnectionPtr &conn){ this->ConnectionWriteComplete(conn); });
        m_Client.SetCanAutoRetry(CanRetry);
    }

    void Start() {
        m_Client.Connect();
    }

    void Send(const std::string& message)
    {
        m_Client.GetConnection()->SendTCP(message);
    }

private:
    void OnRecvMessage(const TcpConnectionPtr & conn, Buffer & buf)
    {
        const auto ret = buf.PopAllDataAsString();
        YLOG_INFO("服务器发来：%s", ret.c_str())
    }

    void ConnectionEstablished(const TcpConnectionPtr& conn) {
        YLOG_INFO("连接至<%s:%d>", conn->GetPeerAddr()->GetIPStr().c_str(), conn->GetPeerAddr()->GetPort())
        m_Client.GetConnection()->SendTCP("你好！");
    }

    void ConnectionWriteComplete(const TcpConnectionPtr& conn) {
        YLOG_INFO("数据已发送给服务器<%s:%d>", conn->GetPeerAddr()->GetIPStr().c_str(), conn->GetPeerAddr()->GetPort())
    }


    TcpClient m_Client;
};





int main()
{
    config::ConfigManager::LoadConfigs();
    EventLoop loop{true};
    IPAddressPtr serverAddr = std::make_shared<IPv4Address>("127.0.0.1", 16666);
    EchoClient echoClient{&loop, serverAddr};
    echoClient.Start();
    loop.Loop();


    return 0;
}