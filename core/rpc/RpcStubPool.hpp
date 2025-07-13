#pragma once

#include "RWLock.h"
#include "TcpConnection.h"
#include "log.h"
#include "EventLoopThread.h"
#include "RpcConnection.h"
#include "ZkServiceManager.h"
#include <google/protobuf/stubs/callback.h>
#include <functional>

namespace yy::core
{

template<typename F>
requires std::invocable<F> // 约束调用F为空参数
class LambdaClosureT final : public google::protobuf::Closure {
public:
    explicit LambdaClosureT(F&& func) : func_(std::move(func)) {}

    void Run() override {
        func_();
        delete this;
    }

private:
    F func_;
};

template<std::invocable<> F> // 约束调用F为空参数
google::protobuf::Closure* NewLambdaClosureT(F&& f) {
    return new LambdaClosureT<F>(std::forward<F>(f));
}


template <typename T>
concept IsValidStub =
    std::derived_from<T, google::protobuf::Service> &&
    requires(T t) {
    { t.channel() } -> std::convertible_to<google::protobuf::RpcChannel*>;
    };



template <IsValidStub ServiceType_Stub>
struct RpcStubConnection
{
    // using F_RpcStubConnectionEstablishedCallback = std::function<void(const std::shared_ptr<RpcStubConnection>& )>;
    explicit RpcStubConnection(yy::net::EventLoop * loop):
        rpc_conn_{new yy::core::RpcConnection(loop)},
        stub_{std::make_unique<ServiceType_Stub>(rpc_conn_, google::protobuf::Service::STUB_OWNS_CHANNEL)}
    {
        assert(rpc_conn_ != nullptr);
        assert(stub_ != nullptr);
    }

    void Connect(const net::IPAddressPtr& server_addr)
    {
        rpc_conn_->Connect(server_addr);
    }

    void Disconnect()
    {
        rpc_conn_->Disconnect();
    }

    ServiceType_Stub& Stub()
    {
        return *stub_;
    }

    void SetConnectionEstablishedCallback(const yy::net::F_ConnectionEstablishedCallback &connectionEstablishedCallback) const
    {
        rpc_conn_->SetConnectionEstablishedCallback(connectionEstablishedCallback);
    }

    std::string GetServiceName()
    {
        return stub_->GetDescriptor()->name();
    }

private:
    yy::core::RpcConnection *           rpc_conn_;
    std::unique_ptr<ServiceType_Stub>   stub_;
};




template <IsValidStub ServiceType_Stub>
class RpcStubPool
{
    using StubType = RpcStubConnection<ServiceType_Stub>;
public:

    explicit RpcStubPool(size_t poolsize, net::F_ConnectionEstablishedCallback cb, const std::string & service_root = "/rpc_services", const bool CanRetry = true) :
        thread_{std::make_unique<yy::net::EventLoopThread>(nullptr, 500ms)},
        loop_{thread_->CreateLoop()},
        service_root_(service_root)
    {
        if (poolsize == 0) { poolsize = 1; }

        zk::ZkServiceManager::Instance().Start(service_root_);

        std::unique_lock lg_aquire(pool_mutex_);
        for (int i = 0; i < poolsize; ++i) {
            std::unique_ptr<StubType> stub_conn = std::make_unique<StubType>(loop_);
            if (i == 0) {
                service_name_ = stub_conn->GetServiceName();
            }
            stub_conn->SetConnectionEstablishedCallback(cb);
            stub_conn->Connect(SelectAddrByRoundRobin());

            pool_.emplace(std::move(stub_conn));
        }
        lg_aquire.unlock();

    }

    void SetServiceChangeCallback (const zk::ZkServiceManager::WatcherCallback & cb)
    {
        m_ServiceChangeCallback = cb;
    }

    void Start()
    {
        assert(m_ServiceChangeCallback);

        // 监听zookeeper在服务根目录下的变化，首次调用时会拉取服务下的所有可用地址
        zk::ZkServiceManager::Instance().Watch(service_name_, [this](const std::string& service_path, std::vector<yy::net::IPAddressPtr> && endpoints) {
            rr_idx_.store(0, std::memory_order_release);

            if (m_ServiceChangeCallback)
                m_ServiceChangeCallback(service_path, std::move(endpoints));
        });
    }

    std::shared_ptr<StubType> Acquire()
    {
        // 1. 等待池中有连接
        std::unique_lock lg_aquire(pool_mutex_);
        pool_cond_.wait(lg_aquire, [this] { return not pool_.empty(); });

        // 2. 从池中取出连接（独占所有权）
        std::unique_ptr<StubType> con_acquire(std::move(pool_.front()));
        pool_.pop();
        lg_aquire.unlock(); // 提前释放锁

        // 3. 构造一个 shared_ptr，带有自定义 deleter，回收时归还到池中
        auto deleter = [this](StubType* con_release) {
            std::unique_lock lg(pool_mutex_);
            pool_.emplace(std::unique_ptr<StubType>(con_release));
            pool_cond_.notify_one();
        };

        return std::shared_ptr<StubType>{con_acquire.release(), deleter};
    }

private:
    auto SelectAddrByRoundRobin() -> yy::net::IPAddressPtr
    {
        const auto endpoints = zk::ZkServiceManager::Instance().FetchLocalCache(service_name_);
        if (endpoints.size() == 0) {
            YLOG_ERROR("服务提供者列表为空，无法执行目标服务！");
            return nullptr;
        }
        const size_t idx = rr_idx_.fetch_add(1, std::memory_order_acq_rel);
        yy::net::IPAddressPtr addr = endpoints[idx % endpoints.size()];
        return addr;
    }

    std::unique_ptr<yy::net::EventLoopThread>       thread_;
    yy::net::EventLoop *                            loop_;
    std::string         service_name_;
    const std::string & service_root_ ;

    std::queue<std::unique_ptr<StubType>>  pool_;
    std::mutex                             pool_mutex_;
    std::condition_variable                pool_cond_;
    std::atomic<size_t>                    rr_idx_;

    zk::ZkServiceManager::WatcherCallback m_ServiceChangeCallback;
};

}
