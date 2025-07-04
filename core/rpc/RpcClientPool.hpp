#pragma once

#include "RWLock.h"
#include "TcpConnection.h"
#include "log.h"
#include "EventLoopThread.h"
#include "RpcConnection.h"
#include "ZkClient.h"
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
    inline static const std::string kServiceRoot = "/services";

    struct RpcClientContext
    {
        explicit RpcClientContext(yy::net::EventLoop * loop) :
            rpc_channel_{new yy::core::RpcConnection(loop)},
            stub_{std::make_unique<ServiceType_Stub>(rpc_channel_, google::protobuf::Service::STUB_OWNS_CHANNEL)}
        {
            assert(rpc_channel_ != nullptr);
            assert(stub_ != nullptr);
        }

        void Connect(const net::IPAddressPtr& server_addr) {
            rpc_channel_->Connect(server_addr);
        }

        void Disconnect() {
            rpc_channel_->Disconnect();
        }

        ServiceType_Stub& Stub()
        {
            return *stub_;
        }

        void SetConnectionEstablishedCallback(const yy::net::F_ConnectionEstablishedCallback &connectionEstablishedCallback) const
        {
            rpc_channel_->SetConnectionEstablishedCallback(connectionEstablishedCallback);
        }

        std::string GetServiceName()
        {
            return stub_->GetDescriptor()->name();
        }

    private:
        yy::core::RpcConnection *           rpc_channel_;
        std::unique_ptr<ServiceType_Stub>   stub_;
    };

    explicit RpcClientPool(size_t poolsize, const bool CanRetry = true) :
        thread_{std::make_unique<yy::net::EventLoopThread>(nullptr, 500ms)},
        loop_{thread_->CreateLoop()},
        zk_client_{std::make_unique<yy::core::ZkClient>()}
    {
        if (poolsize == 0) { poolsize = 1; }

        for (int i = 0; i < poolsize; ++i) {
            auto entry = std::make_unique<RpcClientContext>(loop_);
            entry->SetConnectionEstablishedCallback(m_ConnectionEstablishedCallback);
            pool_.emplace(std::move(entry));
        }
        service_base_ = std::format("{}/{}", kServiceRoot, pool_.back()->GetServiceName());
    }

    void SetConnectionEstablishedCallback (const yy::net::F_ConnectionEstablishedCallback& cb)
    {
        m_ConnectionEstablishedCallback = cb;
    }

    void SetServiceChangeCallback (const std::function<void(std::vector<std::string>&&)> & cb)
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

        yy::net::IPAddressPtr addr{};
        // 获取时连接
        {
            const size_t idx = rr_idx_.fetch_add(1, std::memory_order_acq_rel);
            yy::util::ReadLockGuard lg(endpoints_mutex_);
            addr = endpoints_[idx % endpoints_.size()];
        }

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

        yy::net::IPAddressPtr addr{};
        // 获取时连接
        {
            const size_t idx = rr_idx_.fetch_add(1, std::memory_order_acq_rel);
            yy::util::ReadLockGuard lg(endpoints_mutex_);
            if (endpoints_.size() == 0) {
                YLOG_ERROR("服务提供者列表为空，无法执行目标服务！");
                return nullptr;
            }
            addr = endpoints_[idx % endpoints_.size()];
        }
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
        zk_client_->Start();

        // 监听zookeeper在服务根目录下的变化，首次调用时会拉取所有服务
        zk_client_->AddChildrenWatcher(service_base_, [this](std::vector<std::string> && children) {
            OnServiceChanged(std::move(children));
        });
    }


private:

    void OnServiceChanged(std::vector<std::string> && children) {
        yy::util::WriteLockGuard lg(endpoints_mutex_);
        endpoints_.clear();
        // 获取所有服务的路径
        for (const auto& node : children) {
            // 先做服务发现： 连接zookeeper服务器，获取服务提供方的ip和端口信息
            auto node_path = std::format("{}/{}", service_base_, node);
            YLOG_INFO("[Service Node Change] {}", node_path);
            const auto node_addr = GetServerAddressByServicePath(node_path);

            // 连接服务提供商
            rr_idx_.store(0, std::memory_order_release);
            endpoints_.emplace_back(node_addr);
        }
        lg.unlock();

        if (m_ServiceChangeCallback)
            m_ServiceChangeCallback(std::move(children));
    }

    ///@brief 服务发现
    yy::net::IPAddressPtr GetServerAddressByServicePath(const std::string& service_path)
    {
        const std::string host_str = zk_client_->DiscoverService(service_path);
        if (host_str.empty()) {
            throw std::invalid_argument(std::format("Failed to discover service {}", service_path));
        }
        // 解析服务提供方的ip和端口信息，得到服务提供方地址
        const size_t pos = host_str.find(':');
        if (pos == std::string::npos) {
            throw std::invalid_argument("Invalid host data: " + host_str);
        }
        const std::string ip = host_str.substr(0, pos);
        const std::string port_str = host_str.substr(pos + 1);
        uint16_t port = static_cast<uint16_t>(std::stoi(port_str));
        const auto service_server_addr = std::make_shared<yy::net::IPv4Address>(ip, port);
        return service_server_addr;
    }




    std::unique_ptr<yy::net::EventLoopThread>       thread_;
    yy::net::EventLoop *                            loop_;
    std::string                                     service_base_;

    std::queue<std::unique_ptr<RpcClientContext>>   pool_;
    std::mutex                                      pool_mutex_;
    std::condition_variable                         pool_cond_;

    std::unique_ptr<yy::core::ZkClient>             zk_client_;

    std::atomic<size_t>         rr_idx_;
    std::vector<yy::net::IPAddressPtr>   endpoints_;
    yy::util::RWMutex           endpoints_mutex_;

    yy::net::F_ConnectionEstablishedCallback m_ConnectionEstablishedCallback;
    std::function<void(std::vector<std::string>&&)> m_ServiceChangeCallback;
};

}
