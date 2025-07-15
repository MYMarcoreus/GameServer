#pragma once

#include "TcpConnection.h"
#include "log.h"
#include "EventLoopThread.h"
#include "RpcConnection.h"
#include "ZkServiceManager.h"
#include "EventLoop.h"
#include "Timer.h"
#include "RpcStubConnection.hpp"
#include <google/protobuf/stubs/callback.h>
#include <queue>
#include <functional>

namespace yy::core::rpc
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



template <IsValidStub ServiceType_Stub>
class RpcStubConnectionPool
{
public:
    using StubConnType =  RpcStubConnection<ServiceType_Stub>;

    explicit RpcStubConnectionPool(const size_t poolsize, const std::string & service_root = "/rpc_services", const bool CanRetry = true) :
        thread_{std::make_unique<yy::net::EventLoopThread>(nullptr, 500ms)},
        loop_{thread_->CreateLoop()},
        service_root_(service_root),
        pool_size_{poolsize == 0 ? 1 : poolsize}
    { }

    void SetServiceChangeCallback (const zk::ZkServiceManager::WatcherCallback & cb)
    {
        m_ServiceChangeCallback = cb;
    }

    void Start(typename StubConnType::F_RpcStubConnectionEstablishedCallback cb)
    {
        assert(m_ServiceChangeCallback);

        zkServiceManager_.Start(service_root_);

        for (int i = 0; i < pool_size_; ++i) {
            std::unique_ptr<StubConnType> stub_conn = std::make_unique<StubConnType>(loop_);
            if (i == 0) {
                service_name_ = stub_conn->GetServiceName();
            }

            stub_conn->SetConnectionEstablishedCallback(cb);
            if (auto addr = SelectAddrByRoundRobin(); !addr or not stub_conn->Connect(addr)) {
                YLOG_ERROR("RpcStubPool未找到服务{}", stub_conn->GetServiceName())

                // 在Rpc连接池独占的线程中定时尝试连接服务端
                auto timer = loop_->CreateTimerEvery(1s);
                timer->SetCallback([this, _id = timer->GetID(), _loop = loop_, _cb = cb]()
                {
                    std::unique_ptr<StubConnType> _stub_conn = std::make_unique<StubConnType>(_loop);
                    YLOG_INFO("RpcStubPool重试寻找服务{}", _stub_conn->GetServiceName())
                    _stub_conn->SetConnectionEstablishedCallback(_cb);

                    // 如果找到地址则阻塞在此直到连接成功，将连接加入池中并取消定时器；没找到地址则等待下一次定时器到
                    if (auto _addr = SelectAddrByRoundRobin(); _addr and _stub_conn->Connect(_addr)) {
                        std::unique_lock  _lg(pool_mutex_);
                        pool_.emplace(std::move(_stub_conn));
                        _lg.unlock();
                        _loop->CancelTimer(_id);
                    }
                });
                loop_->AddTimer(timer);
            } else {
                std::lock_guard lg(pool_mutex_);
                pool_.emplace(std::move(stub_conn));
            }
        }

        // 监听zookeeper在服务根目录下的变化，首次调用时会拉取服务下的所有可用地址
        zkServiceManager_.Watch(service_name_, [this](const std::string& service_path, std::vector<yy::net::IPAddressPtr> && endpoints) {
            rr_idx_.store(0, std::memory_order_release);

            if (m_ServiceChangeCallback)
                m_ServiceChangeCallback(service_path, std::move(endpoints));
        });
    }

    std::shared_ptr<StubConnType> Acquire(net::Microseconds delay)
    {
        // 1. 等待池中有连接
        std::unique_lock lg_aquire(pool_mutex_);
        if (!pool_cond_.wait_for(lg_aquire, delay, [this] { return !pool_.empty(); })) {
            // 超时了还没有可用连接
            YLOG_WARN("等待连接超时，pool_仍为空");
            return nullptr;
        }

        // 2. 从池中取出连接（独占所有权）
        std::unique_ptr<StubConnType> con_acquire(std::move(pool_.front()));
        pool_.pop();
        lg_aquire.unlock(); // 提前释放锁

        // 3. 构造一个 shared_ptr，带有自定义 deleter，回收时归还到池中
        auto deleter = [this](StubConnType* con_release) {
            std::unique_lock lg(pool_mutex_);
            pool_.emplace(std::unique_ptr<StubConnType>(con_release));
            pool_cond_.notify_one();
        };

        return std::shared_ptr<StubConnType>{con_acquire.release(), deleter};
    }

private:
    auto SelectAddrByRoundRobin() -> yy::net::IPAddressPtr
    {
        const auto endpoints = zkServiceManager_.FetchLocalCache(service_name_);
        if (endpoints.size() == 0) {
            YLOG_ERROR("服务提供者列表为空，无法执行目标服务！");
            return nullptr;
        }
        const size_t idx = rr_idx_.fetch_add(1, std::memory_order_acq_rel);
        yy::net::IPAddressPtr addr = endpoints[idx % endpoints.size()];
        return addr;
    }

    std::unique_ptr<yy::net::EventLoopThread>   thread_;
    yy::net::EventLoop *                        loop_;
    std::string         service_name_;
    std::string         service_root_ ;

    std::queue<std::unique_ptr<StubConnType>>   pool_;
    std::mutex                                  pool_mutex_;
    std::condition_variable                     pool_cond_;
    size_t                                      pool_size_;
    std::atomic<size_t>                         rr_idx_;

    zk::ZkServiceManager zkServiceManager_;
    zk::ZkServiceManager::WatcherCallback m_ServiceChangeCallback;
};

}
