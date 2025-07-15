#pragma once

#include "AccountRpcClient.h"
#include "IServer.h"
#include "IGameBase.h"
#include "account.pb.h"
#include "ProtobufDispatcher.h"
#include "RpcStubConnectionPool.hpp"
#include "ThreadPool.h"
#include "ZkServiceManager.h"

namespace yy::app::gate
{

///@brief 表示某一类服务的一个连接
class BackendSession
{
public:
    BackendSession(net::EventLoop * loop, const net::IPAddressPtr& serverAddr);

    void Connect(const net::IPAddressPtr& server_addr = nullptr); // 启动连接

    void SendTCP(const          core::MessagePtr & message) const { userconn_->SendTCP(message); }
    void SendTCP(const google::protobuf::Message & message) const { userconn_->SendTCP(message); }
    void SendUDP(const          core::MessagePtr & message) const { userconn_->SendUDP(message); }
    void SendUDP(const google::protobuf::Message & message) const { userconn_->SendUDP(message); }
private:
    void OnBackendResponse(const net::TcpConnectionPtr& conn, const core::MessagePtr & buf);

    net::EventLoop *            loop_;
    net::TcpClient              client_;
    core::ProtobufTcpCodec      codec_;

    core::UserConnectionPtr     userconn_;
};

///@brief 表示某一类服务的所有连接
class BackendGroup {
public:
private:
    std::vector<std::unique_ptr<BackendSession>> backends_;
    std::atomic<size_t> rrIndex_{0}; // round-robin index
};

///@brief 统一管理所有服务类型
class BackendPool {
public:

private:
    std::unordered_map<std::string, BackendGroup> m_backends;
};


class BackendManager {
    const std::string kServiceRoot = "/services";
public:
    BackendManager();

    void Init(net::EventLoop * loop);

    void OnFrontendMessage(const core::UserConnectionPtr & userconn, const core::MessagePtr & message, const core::MessageType type);

private:
    AccountRpcClient    m_accountRpcClient;
    static thread_local BackendPool t_backend;

    //
    core::zk::ZkServiceManager m_zk;
    std::unordered_map<std::string, std::vector<yy::net::IPAddressPtr>> m_routeTable;
};

}
