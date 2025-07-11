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


// class LambdaClosure final : public google::protobuf::Closure {
// public:
//     explicit LambdaClosure(std::function<void()> && func) : func_(std::move(func)) {}
//
//     void Run() override {
//         if (func_) func_();
//         delete this;  // 自毁，等效于 NewCallback 的行为
//     }
//
// private:
//     std::function<void()> func_;
// };
//
// inline google::protobuf::Closure* NewLambdaClosure(std::function<void()> func) {
//     return new LambdaClosure(std::move(func));
// }





template <typename ServiceType>
    requires std::is_base_of_v<google::protobuf::Service, ServiceType>
class RpcClientPool
{
public:
    using ServiceType_Stub = typename ServiceType::Stub;

    struct RpcClientContext
    {
        explicit RpcClientContext(yy::net::EventLoop * loop) :
            rpc_conn_{new yy::core::RpcConnection(loop)},
            stub_{std::make_unique<ServiceType_Stub>(rpc_conn_, google::protobuf::Service::STUB_OWNS_CHANNEL)}
        {
            assert(rpc_conn_ != nullptr);
            assert(stub_ != nullptr);
        }

        void Connect(const net::IPAddressPtr& server_addr) {
            rpc_conn_->Connect(server_addr);
        }

        void Disconnect() {
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

    explicit RpcClientPool(size_t poolsize, const std::string & service_root = "/services", const bool CanRetry = true) :
        thread_{std::make_unique<yy::net::EventLoopThread>(nullptr, 500ms)},
        loop_{thread_->CreateLoop()},
        service_root_(service_root)
    {
        if (poolsize == 0) { poolsize = 1; }

        for (int i = 0; i < poolsize; ++i) {
            auto entry = std::make_unique<RpcClientContext>(loop_);
            entry->SetConnectionEstablishedCallback(m_ConnectionEstablishedCallback);
            if (i == 0) {
                service_name_ = entry->GetServiceName();
            }
            pool_.emplace(std::move(entry));
        }
        zk::ZkServiceManager::Instance().Init(service_root_);
    }

    void SetConnectionEstablishedCallback (const yy::net::F_ConnectionEstablishedCallback& cb)
    {
        m_ConnectionEstablishedCallback = cb;
    }

    void SetServiceChangeCallback (const zk::ZkServiceManager::WatcherCallback & cb)
    {
        m_ServiceChangeCallback = cb;
    }

	std::pair<std::shared_ptr<RpcClientContext>, yy::net::IPAddressPtr> Acquire()
    {
        assert(m_ConnectionEstablishedCallback);

        // 1. 等待池中有连接
        std::unique_lock lg_aquire(pool_mutex_);
        pool_cond_.wait(lg_aquire, [this] { return not pool_.empty(); });

        // 2. 从池中取出连接（独占所有权）
        std::unique_ptr<RpcClientContext> con_acquire(std::move(pool_.front()));
        pool_.pop();
        lg_aquire.unlock(); // 提前释放锁

        const auto endpoints = zk::ZkServiceManager::Instance().FetchLocalCache(service_name_);
        if (endpoints.size() == 0) {
            YLOG_ERROR("服务提供者列表为空，无法执行目标服务！");
            return nullptr;
        }
        const size_t idx = rr_idx_.fetch_add(1, std::memory_order_acq_rel);
        yy::net::IPAddressPtr addr = endpoints[idx % endpoints.size()];

        // 3. 构造一个 shared_ptr，带有自定义 deleter，回收时归还到池中
        auto deleter = [this](RpcClientContext* con_release) {
            std::unique_lock lg(this->pool_mutex_);
            pool_.emplace(std::unique_ptr<RpcClientContext>(con_release));
            pool_cond_.notify_one();
        };

        return {std::shared_ptr<RpcClientContext>{con_acquire.release(), deleter}, addr};
    }

    std::shared_ptr<RpcClientContext> Acquire_AutoConnect()
    {
        assert(m_ConnectionEstablishedCallback);

        // 1. 等待池中有连接
        std::unique_lock lg_aquire(pool_mutex_);
        pool_cond_.wait(lg_aquire, [this] { return not pool_.empty(); });

        // 2. 从池中取出连接（独占所有权）
        std::unique_ptr<RpcClientContext> con_acquire(std::move(pool_.front()));
        pool_.pop();
        lg_aquire.unlock(); // 提前释放锁

        const auto endpoints = zk::ZkServiceManager::Instance().FetchLocalCache(service_name_);
        if (endpoints.size() == 0) {
            YLOG_ERROR("服务提供者列表为空，无法执行目标服务！");
            return nullptr;
        }
        const size_t idx = rr_idx_.fetch_add(1, std::memory_order_acq_rel);
        yy::net::IPAddressPtr addr = endpoints[idx % endpoints.size()];

        // 获取时连接
        con_acquire->Connect(addr);

        // 3. 构造一个 shared_ptr，带有自定义 deleter，回收时归还到池中
        auto deleter = [this](RpcClientContext* con_release) {
            // 释放时关闭连接
            con_release->Disconnect();

            std::unique_lock lg(pool_mutex_);
            pool_.emplace(std::unique_ptr<RpcClientContext>(con_release));
            pool_cond_.notify_one();
        };

        return std::shared_ptr<RpcClientContext>{con_acquire.release(), deleter};
    }



    void Start()
    {
        // 监听zookeeper在服务根目录下的变化，首次调用时会拉取服务下的所有可用地址
        zk::ZkServiceManager::Instance().Watch(service_name_, [this](const std::string& service_path, std::vector<yy::net::IPAddressPtr> && endpoints) {
            rr_idx_.store(0, std::memory_order_release);

            if (m_ServiceChangeCallback)
                m_ServiceChangeCallback(service_path, std::move(endpoints));
        });
    }


private:
    std::unique_ptr<yy::net::EventLoopThread>       thread_;
    yy::net::EventLoop *                            loop_;
    std::string service_name_;
    const std::string & service_root_ ;

    std::queue<std::unique_ptr<RpcClientContext>>   pool_;
    std::mutex                                      pool_mutex_;
    std::condition_variable                         pool_cond_;

    std::atomic<size_t>         rr_idx_;

    yy::net::F_ConnectionEstablishedCallback m_ConnectionEstablishedCallback;
    zk::ZkServiceManager::WatcherCallback m_ServiceChangeCallback;
};

}
