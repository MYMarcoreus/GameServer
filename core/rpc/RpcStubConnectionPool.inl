#pragma once

template <IsValidStub ServiceType_Stub>
RpcStubConnectionPool<ServiceType_Stub>::RpcStubConnectionPool(const size_t poolsize, const std::string& service_root):
    thread_{std::make_unique<net::EventLoopThread>(nullptr, 500ms)},
    loop_{thread_->CreateLoop()},
    service_root_(service_root),
    service_name_{GetServiceName()},
    pool_size_{poolsize}
{ }

template <IsValidStub ServiceType_Stub>
std::string RpcStubConnectionPool<ServiceType_Stub>::GetServiceName()
{
    return ServiceType_Stub::descriptor()->name();
}

template <IsValidStub ServiceType_Stub>
auto RpcStubConnectionPool<ServiceType_Stub>::GetServerNames() -> std::unordered_map<std::string, net::IPAddressPtr>
{
    std::unordered_map<std::string, net::IPAddressPtr> names_and_addrs;
    names_and_addrs.reserve(pool_size_);

    std::unique_lock lg{pool_mutex_};
    for (auto& [name, conn] : pool_) {
        names_and_addrs.emplace(name, conn->GetServerAddr());
    }
    return names_and_addrs;
}

template <IsValidStub ServiceType_Stub>
void RpcStubConnectionPool<ServiceType_Stub>::Start(typename StubConnType::F_RpcStubConnectionEstablishedCallback cb)
{
    assert(serviceChangeCallback_);

    //! 阻塞：进行服务发现
    zkServiceManager_.Start(service_root_);
    while (pool_size_ == 0) {
        zkServiceManager_.FetchRemote(service_name_);
        pool_size_ = zkServiceManager_.EndpointSize(service_name_);
        std::this_thread::sleep_for(200ms);
    }
    auto thread_size = pool_size_;
    if (thread_size > std::thread::hardware_concurrency()) {
        thread_size = std::thread::hardware_concurrency();
    }

    // 启动连接任务线程池
    net::ThreadPool temp_thread_pool("temp connect_thread_pool");
    temp_thread_pool.Start(loop_, thread_size);
    // 连接任务：阻塞直到连接成功
    auto Connect_Task = [this, cb]() -> bool
    {
        return InsertConn(SelectAddrByRoundRobin(), cb);
    };
    YLOG_INFO("{}RPC连接池大小 = {}", GetServiceName(), pool_size_)

    // 启动并发连接任务，n个连接启动n个线程，每个线程负责一个连接定时任务
    for (int i = 0; i <  pool_size_ ;++i) {
        // 循环任务控制，若任务完成则取消循环任务，否则继续执行该任务
        auto fun = [Connect_Task] {
            while (Connect_Task() == false) {
                YLOG_ERROR("RpcStubPool连接失败，再次尝试连接{}", GetServiceName())
            }
        };
        temp_thread_pool.PushTask(fun);
    }

    // 阻塞直到所有连接完成
    while (temp_thread_pool.TaskQueueSize() > 0) { /* spin */ }

    // 监听zookeeper在服务根目录下的变化
    zkServiceManager_.Watch(service_name_,
        [this, cb](const std::string& service_base, std::unordered_map<std::string, net::IPAddressPtr> endpoints)
        {
            rr_idx_.store(0, std::memory_order_release);

            for (auto& [name, addr] : endpoints)
            {
                // 是有新节点上线，连接之
                this->InsertConn(addr, cb);
                // 这里不检测旧节点下线，因为启动了TcpClient的自动重连
                YLOG_INFO("endpoints: {}-{}", name, addr->ToString())
            }

            if (serviceChangeCallback_)
                serviceChangeCallback_(service_base, std::move(endpoints));
        });
}

template <IsValidStub ServiceType_Stub>
std::shared_ptr<typename RpcStubConnectionPool<ServiceType_Stub>::StubConnType> RpcStubConnectionPool<ServiceType_Stub>
::Acquire_Random(net::Microseconds delay)
{
    // 1. 等待池中有连接
    std::unique_lock lg_aquire(pool_mutex_);
    if (!pool_cond_.wait_for(lg_aquire, delay, [this] { return !pool_.empty(); })) {
        // 超时了还没有可用连接
        YLOG_WARN("等待连接超时，pool_仍为空");
        return nullptr;
    }

    // 2. 从池中取出连接（独占所有权）
    auto it = pool_.begin();
    std::unique_ptr<StubConnType> con_acquire = std::move(it->second);
    pool_.erase(it); // 从 map 中移除该元素
    lg_aquire.unlock(); // 释放锁

    // 3. 构造一个 shared_ptr，带有自定义 deleter，回收时归还到池中
    auto deleter = [this](StubConnType* con_release) {
        std::unique_lock lg(pool_mutex_);
        pool_.emplace(con_release->GetServerName(), std::unique_ptr<StubConnType>(con_release));
        pool_cond_.notify_one();
    };

    return std::shared_ptr<StubConnType>{con_acquire.release(), deleter};
}

template <IsValidStub ServiceType_Stub>
std::shared_ptr<typename RpcStubConnectionPool<ServiceType_Stub>::StubConnType> RpcStubConnectionPool<ServiceType_Stub>
::Acquire_From(std::string server_name, net::Microseconds delay)
{
    // 1. 等待池中有连接
    std::unique_lock lg_aquire(pool_mutex_);
    if (!pool_cond_.wait_for(lg_aquire, delay, [this] { return !pool_.empty(); })) {
        // 超时了还没有可用连接
        YLOG_WARN("等待连接超时，pool_仍为空");
        return nullptr;
    }

    // 2. 从池中取出连接（独占所有权）
    auto it = pool_.find(server_name);
    if (it == pool_.end()) {
        return nullptr;
    }
    std::unique_ptr<StubConnType> con_acquire = std::move(it->second);
    pool_.erase(it); // 从 map 中移除该元素
    lg_aquire.unlock(); // 释放锁

    // 3. 构造一个 shared_ptr，带有自定义 deleter，回收时归还到池中
    auto deleter = [this](StubConnType* con_release) {
        std::unique_lock lg(pool_mutex_);
        pool_.emplace(con_release->GetServerName(), std::unique_ptr<StubConnType>(con_release));
        pool_cond_.notify_one();
    };

    return std::shared_ptr<StubConnType>{con_acquire.release(), deleter};
}

template <IsValidStub ServiceType_Stub>
auto RpcStubConnectionPool<ServiceType_Stub>::SelectAddrByRoundRobin() -> net::IPAddressPtr
{
    const auto endpoints = zkServiceManager_.FetchRemote(service_name_);
    if (endpoints.size() == 0) {
        YLOG_ERROR("服务提供者列表为空，无法执行目标服务！");
        return nullptr;
    }
    const size_t idx = rr_idx_.fetch_add(1, std::memory_order_acq_rel);
    net::IPAddressPtr addr = endpoints[idx % endpoints.size()];
    return addr;
}


template <IsValidStub ServiceType_Stub>
bool RpcStubConnectionPool<ServiceType_Stub>::InsertConn(net::IPAddressPtr addr, typename StubConnType::F_RpcStubConnectionEstablishedCallback cb)
{
    // 服务器名称 = IP:Port
    auto server_name = std::format("{}:{}", addr->GetIPStr(), addr->GetPort());
    //todo 一个节点只对应一个连接，不重复插入，后续可以考虑一个节点对应多个连接
    if (pool_.contains(server_name)){
        return false;
    }
    // 非法的服务地址
    if (addr == nullptr) {
        YLOG_ERROR("RpcStubPool未找到服务{}", StubConnType::GetServiceName())
        return false;
    }

    // 创建连接并连接至服务提供方
    std::unique_ptr<StubConnType> stub_conn = std::make_unique<StubConnType>(loop_);
    stub_conn->SetConnectionEstablishedCallback(std::move(cb));
    if (not stub_conn->Connect(addr)) {
        YLOG_ERROR("RpcStubPool无法连接服务{}", stub_conn->GetServiceName())
        return false;
    }

    // 连接成功将连接加入连接池中
    std::unique_lock lg(pool_mutex_);
    pool_.emplace(server_name, std::move(stub_conn));
    return true;
}
