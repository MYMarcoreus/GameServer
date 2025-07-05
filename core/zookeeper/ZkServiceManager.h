#pragma once

#include "net_definations.h"
#include "RWLock.h"
#include "Singleton.h"
#include "ZkClient.h"

namespace yy::core::zk
{

class ZkServiceManager final : public Singleton<ZkServiceManager> {
    SINGLETON_NECESSITY(ZkServiceManager)
    mutable std::once_flag  zk_client_init_flag_;
    using WatcherCallback = std::function<void(std::vector<yy::net::IPAddressPtr> &&)>;
public:
    void Init(const std::string & service_root);

    // 注册服务（服务名 + 实例地址）
    bool Register(const std::string& service_name, const std::string& ip, const std::string& port);

    // 注销服务
    // void Unregister();

    ///@brief 服务发现
    std::vector<yy::net::IPAddressPtr> FetchLocalCache(const std::string& service_name);
    std::vector<yy::net::IPAddressPtr> FetchRemote(const std::string& service_name);

    void Watch(const std::string& service_name, WatcherCallback && cb);

private:
    std::vector<yy::net::IPAddressPtr> StrEndpointsToIpAddr(const std::string& service_base, std::vector<std::string>&& children);

private:
    ZkClient zk_client_;
    std::string     service_root_;
    std::unordered_map<std::string, std::vector<yy::net::IPAddressPtr>> all_endpoints_;
    yy::util::RWMutex                                                   endpoints_mutex_;
};

}
