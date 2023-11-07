#include "TcpClient.h"
#include "TcpConnection.h"
#include "EventLoop.h"
#include "IPAddress.h"
#include "log.h"
#include <stdio.h>

using namespace yy;
using namespace yy::net;
using namespace yy::util;
using std::string;



class EchoClient
{
public:
    EchoClient(EventLoop * loop, IPAddressPtr serverAddr, bool CanRetry = true)
        : m_Client(loop, serverAddr)
    {
        m_Client.SetMessageCallback(std::bind(&EchoClient::OnRecvMessage, this, _1, _2));
        m_Client.SetConnectionEstablishedCallback(std::bind(&EchoClient::ConnectionEstablished, this, _1));
        m_Client.SetConnectionWriteCompleteCallback(std::bind(&EchoClient::ConnectionWriteComplete, this, _1));
        m_Client.SetCanAutoRetry(CanRetry);
    }

    void Start() {
        m_Client.Connect();
    }

    void Send(std::string message)
    {
        m_Client.GetConnection()->Send(message);
    }

private:
    void OnRecvMessage(const TcpConnectionPtr & conn, Buffer & buf)
    {
        auto ret = buf.RetrieveAllDataAsString();
        YLOG_INFO("服务器发来：%s", ret.c_str())
    }

    void ConnectionEstablished(TcpConnectionPtr conn) {
        YLOG_INFO("连接至<%s:%d>", conn->GetPeerAddr()->GetIPStr().c_str(), conn->GetPeerAddr()->GetPort())
        m_Client.GetConnection()->Send("你好！");
    }

    void ConnectionWriteComplete(TcpConnectionPtr conn) {
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