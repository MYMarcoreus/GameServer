#include "ZkServiceClient.h"

#include "IPAddress.h"
#include "log.h"
#include "ZkClient.h"
#include <algorithm>

namespace yy::core::zk
{
ZkServiceClient::ZkServiceClient(): zk_client_(std::make_unique<ZkClient>())
{ }

ZkServiceClient::~ZkServiceClient()
{ }

void ZkServiceClient::Start(const std::string& service_root)
{
    std::call_once(zk_client_init_flag_, [this, &service_root]() {
        service_root_ = service_root;
        zk_client_->Start();
        zk_client_->CreateNode(service_root_);
        FetchAllRemote();
    });
}

bool ZkServiceClient::Register(const std::string& service_name, const std::string& ip, const std::string& port)
{
    // 声明：构建服务节点路径与实例路径
    const std::string service_node_path = std::format("{}/{}", service_root_, service_name);
    const std::string host_info = std::format("{}:{}", ip, port);
    const std::string instance_node_path = std::format("{}/{}", service_node_path, host_info);

    do {
        //! ① 创建服务节点（父目录，如 /services/AccountServiceRpc，若不存在）
        zk_client_->CreateNode(service_node_path);

        //! ② 注册服务实例为临时节点（会话断开自动删除）
        zk_client_->CreateNode(instance_node_path, host_info, ZOO_EPHEMERAL);
    } while (false);

    //! 统一出口：创建完成后立即拉取远端信息，更新本地缓存
    FetchRemote(service_name);
    return true;
}

void ZkServiceClient::Unregister(const std::string& service_name, const std::string& ip, const std::string& port)
{
    const std::string service_node_path = std::format("{}/{}", service_root_, service_name);
    const std::string host_info = std::format("{}:{}", ip, port);
    const std::string instance_node_path = std::format("{}/{}", service_node_path, host_info);
    zk_client_->DeleteNode(instance_node_path);
}


auto ZkServiceClient::FetchLocalCache(const std::string& service_name) -> std::vector<net::IPAddressPtr>
{
    const auto service_node_path = std::format("{}/{}", service_root_, service_name);

    // Fetch的是本地缓存
    std::vector<net::IPAddressPtr> endpoints;
    {
        util::ReadLockGuard lg(service_endpoint_mutex_);
        if (const auto it = service_endpoint_map_.find(service_node_path); it != service_endpoint_map_.end()) {
            endpoints = it->second;
        }
    }
    return endpoints;
}

auto ZkServiceClient::FetchAllLocalCache() -> std::unordered_map<std::string, std::vector<net::IPAddressPtr>>
{
    util::ReadLockGuard lg(service_endpoint_mutex_);
    return service_endpoint_map_;
}

auto ZkServiceClient::FetchRemote(const std::string& service_name) -> std::vector<net::IPAddressPtr>
{
    const auto service_node_path = std::format("{}/{}", service_root_, service_name);
    auto endpoints = StrEndpointsToIpAddr(service_node_path, zk_client_->GetNodeChildren(service_node_path));
    {
        util::WriteLockGuard lg(service_endpoint_mutex_);
        service_endpoint_map_[service_node_path] = std::move(endpoints);
        return service_endpoint_map_[service_node_path];
    }
}

auto ZkServiceClient::FetchAllRemote() -> std::unordered_map<std::string, std::vector<net::IPAddressPtr>>
{
    std::unordered_map<std::string, std::vector<net::IPAddressPtr>> new_map;
	const std::vector<std::string> service_names = zk_client_->GetNodeChildren(service_root_);
    for (const auto& service_name : service_names) {
        const auto service_node_path = std::format("{}/{}", service_root_, service_name);
        std::vector<std::string> providers = zk_client_->GetNodeChildren(service_node_path);
        std::ranges::for_each(providers, [&](const auto& provider_str) {
            YLOG_INFO("[ZkServiceClient] Fetch Remote Service {}: {}", service_node_path, provider_str);
        });
        const std::vector<net::IPAddressPtr> endpoints = StrEndpointsToIpAddr(service_node_path, providers);
        new_map[service_node_path] = endpoints;
    }

    {
        util::WriteLockGuard lg(service_endpoint_mutex_);
        service_endpoint_map_ = std::move(new_map);
        return service_endpoint_map_;
    }
}

size_t ZkServiceClient::EndpointSize(const std::string& service_name)
{
    const auto service_node_path = std::format("{}/{}", service_root_, service_name);

    util::ReadLockGuard lg(service_endpoint_mutex_);
    const auto it = service_endpoint_map_.find(service_node_path);
    return it != service_endpoint_map_.end() ? it->second.size() : 0;
}


void ZkServiceClient::Watch(const std::string& service_name, WatcherCallback watcher_cb)
{
    auto service_node_path = std::format("{}/{}", service_root_, service_name);

    // 监听zookeeper服务的变化
    zk_client_->AddChildrenWatcher(service_node_path,
        [this, watcher_cb = std::move(watcher_cb), service_node_path]
        (const std::string& path, const std::vector<std::string> & providers)
        {
            assert(service_node_path == path);
            // 获取服务的地址
            const std::vector<net::IPAddressPtr> endpoints = StrEndpointsToIpAddr(service_node_path, providers);
            std::unordered_map<std::string, net::IPAddressPtr> M;
            for (int i = 0; i < providers.size() ;++i) {
                M[providers[i]] = endpoints[i];
            }

            // 更新本地缓存
            {
                util::WriteLockGuard lg(service_endpoint_mutex_);
                service_endpoint_map_[service_node_path] = endpoints;
            }

            // 调用上层回调
            watcher_cb(service_node_path, std::move(M));
        });
}

auto ZkServiceClient::StrEndpointsToIpAddr(const std::string& service_base, const std::vector<std::string> & providers) -> std::vector<net::IPAddressPtr>
{
    std::vector<net::IPAddressPtr> endpoints;

    for (const auto& provider : providers) {
        // 声明
        const std::string service_node_path = std::format("{}/{}", service_base, provider);
        std::string host_str;
        std::string error_msg;

        do {
            //! ① 服务发现：获取服务提供方的 ip 和端口信息
            host_str = zk_client_->GetNodeData(service_node_path);
            if (host_str.empty()) {
                error_msg = std::format("Failed to discover service {}", service_node_path);
                break;
            }

            //! ② 解析 ip:port
            const size_t pos = host_str.find(':');
            if (pos == std::string::npos) {
                error_msg = std::format("Invalid host data: {}", host_str);
                break;
            }
            const std::string ip = host_str.substr(0, pos);
            const std::string port_str = host_str.substr(pos + 1);
            const uint16_t port = static_cast<uint16_t>(std::stoi(port_str));

            //! ③ 构造服务提供方地址
            endpoints.emplace_back(std::make_shared<net::IPv4Address>(ip, port));
        } while (false);

        //! 统一出口：解析失败时抛出异常
        if (!error_msg.empty()) {
            throw std::invalid_argument(error_msg);
        }
    }
    return endpoints;
}

}
