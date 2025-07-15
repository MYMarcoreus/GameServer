#pragma once

#include "ZkClient.h"
#include "net_definations.h"
#include "RWLock.h"

namespace yy::core::zk
{

class ZkServiceManager  {
    mutable std::once_flag  zk_client_init_flag_;
public:
    using WatcherCallback = std::function<void(const std::string&, std::vector<yy::net::IPAddressPtr> &&)>;
    void Start(const std::string & service_root);

    // 注册服务（服务名 + 实例地址）
    bool Register(const std::string& service_name, const std::string& ip, const std::string& port);

    // 注销服务
    // void Unregister();

    ///@brief 服务发现
    auto FetchLocalCache(const std::string& service_name) -> std::vector<yy::net::IPAddressPtr>;
    auto FetchAllLocalCache() -> std::unordered_map<std::string, std::vector<yy::net::IPAddressPtr>>;
    auto FetchRemote(const std::string& service_name) -> std::vector<yy::net::IPAddressPtr>;
    auto FetchAllRemote() -> std::unordered_map<std::string, std::vector<yy::net::IPAddressPtr>>;

    void Watch(const std::string& service_name, WatcherCallback && cb);

private:
    auto StrEndpointsToIpAddr(const std::string& service_base, std::vector<std::string>&& providers) -> std::vector<yy::net::IPAddressPtr>;

private:
    ZkClient zk_client_;
    std::string     service_root_;
    std::unordered_map<std::string, std::vector<yy::net::IPAddressPtr>> service_endpoint_map_; // 服务地址本地缓存
    yy::util::RWMutex                                                   service_endpoint_mutex_;
};

}
