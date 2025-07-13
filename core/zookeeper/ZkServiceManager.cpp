#include "ZkServiceManager.h"
#include "IPAddress.h"
#include "log.h"
#include <format>

namespace yy::core::zk
{
void ZkServiceManager::Start(const std::string& service_root)
{
    std::call_once(zk_client_init_flag_, [this, &service_root]() {
        service_root_ = service_root;
        zk_client_.Start();
        zk_client_.CreateNode(service_root_);
        FetchAllRemote();
    });
}

bool ZkServiceManager::Register(const std::string& service_name, const std::string& ip, const std::string& port)
{
    auto service_base = std::format("{}/{}", service_root_, service_name);

    // /services/AccountServiceRpc
    zk_client_.CreateNode(service_base);

    // /services/AccountServiceRpc/provider00000001 存储当前这个rpc服务节点主机的ip和port
    const auto ip_port = std::format("{}:{}", ip, port);
    const std::string service_path = std::format("{}/{}", service_base, ip_port);
    zk_client_.CreateNode(service_path, ip_port,  ZOO_EPHEMERAL); // ZOO_EPHEMERAL：表示znode是一个临时性节点

    // FetchRemote 创建后从远端获取，更新本地缓存
    FetchRemote(service_name);

    return true;
}

auto ZkServiceManager::FetchLocalCache(const std::string& service_name) -> std::vector<yy::net::IPAddressPtr>
{
    const auto service_base = std::format("{}/{}", service_root_, service_name);

    // Fetch的是本地缓存
    std::vector<yy::net::IPAddressPtr> endpoints;
    {
        yy::util::ReadLockGuard lg(service_endpoint_mutex_);
        if (const auto it = service_endpoint_map_.find(service_base); it != service_endpoint_map_.end()) {
            endpoints = it->second;
        }
    }
    return endpoints;
}

auto ZkServiceManager::FetchAllLocalCache() -> std::unordered_map<std::string, std::vector<yy::net::IPAddressPtr>>
{
    yy::util::ReadLockGuard lg(service_endpoint_mutex_);
    return service_endpoint_map_;
}

auto ZkServiceManager::FetchRemote(const std::string& service_name) -> std::vector<yy::net::IPAddressPtr>
{
    const auto service_base = std::format("{}/{}", service_root_, service_name);
    const auto endpoints = StrEndpointsToIpAddr(service_base, zk_client_.GetNodeChildren(service_base));
    {
        yy::util::WriteLockGuard lg(service_endpoint_mutex_);
        service_endpoint_map_[service_base] = std::move(endpoints);
        return service_endpoint_map_[service_base];
    }
}

auto ZkServiceManager::FetchAllRemote() -> std::unordered_map<std::string, std::vector<yy::net::IPAddressPtr>>
{
    std::unordered_map<std::string, std::vector<yy::net::IPAddressPtr>> new_map;
	const std::vector<std::string> service_names = zk_client_.GetNodeChildren(service_root_);
    for (const auto& service_name : service_names) {
        const auto service_base = std::format("{}/{}", service_root_, service_name);
        std::vector<std::string> providers = zk_client_.GetNodeChildren(service_base);
        const std::vector<yy::net::IPAddressPtr> endpoints = StrEndpointsToIpAddr(service_base, std::move(providers));
        new_map[service_base] = endpoints;
    }

    {
        yy::util::WriteLockGuard lg(service_endpoint_mutex_);
        service_endpoint_map_ = std::move(new_map);
        return service_endpoint_map_;
    }
}


void ZkServiceManager::Watch(const std::string& service_name, WatcherCallback && cb)
{
    auto service_base = std::format("{}/{}", service_root_, service_name);

    // 监听zookeeper服务的变化，首次调用时会拉取所有地址
    zk_client_.AddChildrenWatcher(service_base, [this, cb = std::move(cb), service_base](const std::string& path, std::vector<std::string>&& providers) {
        assert(service_base == path);
        // 获取服务的地址
        std::vector<yy::net::IPAddressPtr> endpoints = StrEndpointsToIpAddr(service_base, std::move(providers));

        // 更新本地缓存
        {
            yy::util::WriteLockGuard lg(service_endpoint_mutex_);
            service_endpoint_map_[service_base] = endpoints;
        }

        // 调用上层回调
        cb(service_base, std::move(endpoints));
    });
}

auto ZkServiceManager::StrEndpointsToIpAddr(const std::string& service_base, std::vector<std::string>&& providers) -> std::vector<yy::net::IPAddressPtr>
{
    std::vector<yy::net::IPAddressPtr> endpoints;
    for (const auto& provider : providers) {
        // 先做服务发现： 连接zookeeper服务器，获取服务提供方的ip和端口信息
        auto service_path = std::format("{}/{}", service_base, provider);
        YLOG_INFO("[Fetch Remote Service] {}", service_path);

        const std::string host_str = zk_client_.GetNodeVal(service_path);
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
        const auto service_addr = std::make_shared<yy::net::IPv4Address>(ip, port);

        endpoints.emplace_back(service_addr);
    }
    return endpoints;
}

}
