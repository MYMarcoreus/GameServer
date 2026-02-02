#pragma once

#include "net_definations.h"
#include "RWLock.h"

namespace yy::core::zk
{
class ZkClient;

class ZkServiceClient  {
    mutable std::once_flag  zk_client_init_flag_;
public:
    using WatcherCallback = std::function<void(const std::string&, std::unordered_map<std::string, net::IPAddressPtr>)>;

    ZkServiceClient();
    ~ZkServiceClient();

    void Start(const std::string & service_root);

    ///@brief 服务注册（服务名 + 实例地址ip:port）
    bool Register(const std::string& service_name, const std::string& ip, const std::string& port);

    ///@brief 服务发现
    auto FetchLocalCache(const std::string& service_name) -> std::vector<net::IPAddressPtr>;
    auto FetchAllLocalCache() -> std::unordered_map<std::string, std::vector<net::IPAddressPtr>>;
    auto FetchRemote(const std::string& service_name) -> std::vector<net::IPAddressPtr>;
    auto FetchAllRemote() -> std::unordered_map<std::string, std::vector<net::IPAddressPtr>>;
    size_t EndpointSize(const std::string& service_name);

    ///@brief 服务监听
    ///@param service_name 监听的服务名
    ///@param watcher_cb 监听回调
    void Watch(const std::string& service_name, WatcherCallback watcher_cb);

private:
    auto StrEndpointsToIpAddr(const std::string& service_base, const std::vector<std::string> & providers) -> std::vector<net::IPAddressPtr>;

private:
    std::unique_ptr<ZkClient> zk_client_;
    std::string service_root_;
    std::unordered_map<std::string, std::vector<net::IPAddressPtr>> service_endpoint_map_; // 服务地址本地缓存
    util::RWMutex service_endpoint_mutex_;
};

}
