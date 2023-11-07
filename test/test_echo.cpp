#include "TcpServer.h"
#include "TcpConnection.h"
#include "EventLoop.h"
#include "Acceptor.h"
#include "IPAddress.h"
#include "log.h"
#include "ThreadPool.h"

#include <stdio.h>

#include "player.pb.h"

using namespace yy;
using namespace yy::net;
using namespace yy::util;
using std::string;



class EchoServer
{
public:
    EchoServer(EventLoop* loop, IPAddressPtr listenAddr)
        : server_(loop, listenAddr, true)
    {
        server_.SetConnectionEstablishedCallback( std::bind(&EchoServer::OnConnectionEstablished, this, _1));
        server_.SetMessageCallback( std::bind(&EchoServer::onMessage, this, _1, _2));
    }

    void Start()
    {
        workThreads_.Start(2); // 工作线程
        server_.Start(2);      // IO线程
    }

private:
    void OnConnectionEstablished(TcpConnectionPtr conn)
    {
        YLOG_INFO("██████████████████████████████████████████████████████连接成功！")
    }

    /* ! 注意：在onMessage中，不要把recvBuf的引用或指针作为参数传递给另一线程（如线程池中的线程），
       ! onMessage需在的调用者线程中操作recvBuf， 否则可能在成recvBuf的线程不安全 */
    void onMessage(TcpConnectionPtr conn, Buffer& recvBuf)
    {
        YLOG_INFO("▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲处理用户<%d: %s>的消息<%zu>",
                  conn->GetSocketFD(), conn->GetName().c_str(), recvBuf.GetDataSize())

        std::string message = recvBuf.RetrieveAllDataAsString();

        workThreads_.PushTask([message, conn](){
            //! 错误的！不要在另一线程中操作recvBuf
            // std::string message = recvBuf.RetrieveAllDataAsString();
            printf("消息为：%s\n", message.c_str());
            conn->Send(message);
        });
    }

    TcpServer  server_;
    ThreadPool workThreads_;
};






int main()
{
    config::ConfigManager::LoadConfigs();
    EventLoop loop{true};
    IPAddressPtr listenAddr = std::make_shared<IPv4Address>(config::g_app_config->GetValue().app_port());
    EchoServer server(&loop, listenAddr);
    server.Start();
    loop.Loop();


    return 0;
}
