#pragma once

#include "ZkServiceClient.h"
#include "TcpConnection.h"
#include "log.h"
#include "EventLoopThread.h"
#include "RpcConnection.h"
#include "RpcStubConnection.hpp"
#include <queue>
#include <functional>

#include "ThreadPool.h"

namespace yy::core::rpc
{

template <IsValidStub ServiceType_Stub>
class RpcStubConnectionPool
{
public:
    using StubConnType =  RpcStubConnection<ServiceType_Stub>;

    explicit RpcStubConnectionPool(size_t poolsize, const std::string & service_root = "/rpc_services");

    explicit RpcStubConnectionPool(const std::string & service_root = "/rpc_services")
        : RpcStubConnectionPool(0, service_root)
    { }

    ///@brief 获取服务名
    static std::string GetServiceName();

    ///@brief 获取所有可选的服务提供方的名称和地址
    auto GetServerNames() -> std::unordered_map<std::string, net::IPAddressPtr>
    ;

    ///@brief 设置服务上线和下线回调
    void SetServiceChangeCallback (const zk::ZkServiceClient::WatcherCallback & cb) { m_ServiceChangeCallback = cb; }

    ///@brief 阻塞连接：
    /// 阻塞点(1) 一直服务发现直到服务上线；
    /// 阻塞点(2). 阻塞同步连接服务提供方。
    void Start(StubConnType::F_RpcStubConnectionEstablishedCallback cb);

    std::shared_ptr<StubConnType> Acquire_Random(net::Microseconds delay);

    ///@brief 根据服务器名称，获取指定的服务器
    std::shared_ptr<StubConnType> Acquire_From(std::string server_name, net::Microseconds delay);

private:
    auto SelectAddrByRoundRobin() -> net::IPAddressPtr
    ;

    std::unique_ptr<net::EventLoopThread>   thread_;
    net::EventLoop *                        loop_;
    std::string         service_root_ ;
    std::string         service_name_;

    std::unordered_map<std::string, std::unique_ptr<StubConnType>>  pool_;
    std::mutex                                                      pool_mutex_;
    std::condition_variable                                         pool_cond_;
    size_t                                                          pool_size_;
    std::atomic<size_t>                                             rr_idx_;

    zk::ZkServiceClient zkServiceManager_;
    zk::ZkServiceClient::WatcherCallback m_ServiceChangeCallback;
};

#include "RpcStubConnectionPool.inl"
}
