#include "ZkClient.h"
#include "log.h"
#include "ConfigManager.h"
#include "RemoteXmlConfig.h"
#include <semaphore.h>
#include <iostream>

namespace yy::core::zk
{


void ZkClient::global_watcher(zhandle_t* zh, int type, int state, const char* node_path, void* watcherCtx)
{
    const auto zk_client = static_cast<ZkClient*>(watcherCtx);

    if (type == ZOO_SESSION_EVENT) {
        if (state == ZOO_CONNECTED_STATE) {
            zk_client->connected_.store(true, std::memory_order_release);
            zk_client->connected_.notify_all();
            YLOG_INFO("[ZkWatcher] Connect Success to ZooKeeper!");
        } else if (state == ZOO_EXPIRED_SESSION_STATE) {
            YLOG_WARN("[ZkWatcher] Session expired! Need to reconnect.");
            zk_client->Stop();
            zk_client->Start();
            zk_client->RecoverEphemeralNodes();
        } else if (state == ZOO_CONNECTING_STATE) {
            YLOG_INFO("[ZkWatcher] Connecting to ZooKeeper.");
        } else if (state == ZOO_AUTH_FAILED_STATE) {
            YLOG_WARN("[ZkWatcher] Authentication failed.");
        } else {
            YLOG_WARN("[ZkWatcher] Other session state: {}", state);
        }
    }
}



ZkClient::ZkClient(const std::string & host) : host_(host), zhandle_(nullptr)
{
}

ZkClient::~ZkClient()
{
    Stop();
}

// 连接zkserver
void ZkClient::Start()
{
    if (host_ == "") {
        std::string ip;
        std::string port;
        for (auto & node: config::g_remote_config->GetValue().m_remote_nodes) {
            if (node.type == "zookeeper") {
                ip = node.ip;
                port = std::to_string(node.port);
            }
        }
        if (ip.empty() && port.empty()) {
            std::cerr << "未配置ZooKeeper的IP地址或端口，结束程序！" << std::endl;
            std::terminate();
        }
        const std::string zk_host = std::format("{}:{}", ip, port);
        host_ = zk_host;
    }
    if (zhandle_) {
        YLOG_DEBUG("[ZkClient] zookeeper_init repeated initialization!");
        return;
    }

    zhandle_ = zookeeper_init(host_.c_str(), global_watcher, 30000, nullptr, this, 0);
    if (nullptr == zhandle_)
    {
        YLOG_FATAL("[ZkClient] zookeeper_init error!");
        exit(EXIT_FAILURE);
    }

    // 阻塞直到连接成功
    while (!connected_.load(std::memory_order_acquire)) { // while防止虚假唤醒：wait被唤醒后，值不一定改变，因为唤醒方不一定修改值
        connected_.wait(false, std::memory_order_acquire);
    }

    YLOG_INFO("[ZkClient] zookeeper_init success!");
}

void ZkClient::Stop()
{
    if (zhandle_ != nullptr) {
        const int err = zookeeper_close(zhandle_); // 关闭句柄，释放资源
        if (err == ZOK) {
            connected_.store(false, std::memory_order_release);
        }
        zhandle_ = nullptr;
    }
}

void ZkClient::CreateNode(const std::string& node_path, const std::string& node_data, int flags)
{
    if (zhandle_ == nullptr) {
        this->Start();
    }

    // 判断 node_path 对应的 znode 节点是否存在
    int errcode = zoo_exists(zhandle_, node_path.c_str(), 0, nullptr);
    if (errcode != ZNONODE) {
        if (flags & ZOO_EPHEMERAL) {
            // 如果存在的结点为临时节点，则删除之；
            errcode = zoo_delete(zhandle_, node_path.c_str(), -1);
            if (errcode != ZOK) {
                YLOG_ERROR("[ZkClient] failed to delete existing ephemeral znode: {}, code: {}", node_path, errcode);
                return;
            }
        } else {
            // 存在持久节点时，直接返回
            return;
        }
    }

    // 创建节点
    char path_buffer[128] {};
    constexpr int bufferlen = sizeof(path_buffer);
    errcode = zoo_create(zhandle_, node_path.c_str(), node_data.c_str(), node_data.length(),
                         &ZOO_OPEN_ACL_UNSAFE, flags, path_buffer, bufferlen);
    if (errcode == ZOK) {
        YLOG_INFO("[ZkClient] znode create success... <node_path: {}, node_data: {}>", node_path, node_data);
        // 如果是临时节点，记录该节点以便断线重连时恢复
        if (flags & ZOO_EPHEMERAL) {
            ephemeral_nodes_.emplace_back(node_path, node_data, flags);
        }
    } else {
        YLOG_FATAL("[ZkClient] znode create error... <{}:{}>, error: {}", node_path, node_data, errcode);
    }
}


void ZkClient::RecoverEphemeralNodes()
{
    for (const auto& [path, data, flags] : ephemeral_nodes_) {
        int errcode = zoo_exists(zhandle_, path.c_str(), 0, nullptr);
        if (errcode == ZNONODE) {
            char path_buffer[128]{};
            constexpr int buffer_len = sizeof(path_buffer);
            errcode = zoo_create(zhandle_, path.c_str(), data.c_str(), data.length(),
                &ZOO_OPEN_ACL_UNSAFE, flags, path_buffer, buffer_len);
            if (errcode == ZOK) {
                YLOG_INFO("[ZKClient] Recovered ephemeral node: {}", path);
            } else {
                YLOG_FATAL("[ZKClient] Failed to recover node: {}, error: {}", path, errcode);
            }
        }
    }
}

// 根据指定的path，获取znode节点的值
std::string ZkClient::GetNodeData(const std::string& node_path)
{
    if (zhandle_ == nullptr) {
        this->Start();
    }

    //! zoo_get是线程安全的，但是buffer等传入的数据结构需要用户保证线程安全，此处使用栈变量，能保证每个线程一个变量，保证线程安全。
    char buffer[64]{};
    int bufferlen = sizeof(buffer);
    const int flag = zoo_get(zhandle_, node_path.c_str(), 0, buffer, &bufferlen, nullptr);
    if (flag != ZOK)
    {
        YLOG_WARN("[ZkClient] get znode error... node_path: {}", node_path);
        return "";
    }
    else
    {
        return std::string(buffer, bufferlen);
    }
}




void ZkClient::AddChildrenWatcher(const std::string& node_path, const bool trigger_now, WatcherCallback callback)
{
    {
        std::unique_lock lock(watcher_cb_mutex_);
        child_watch_callbacks_[node_path] = std::move(callback);
    }

    // 拉取当前子节点并设置 watcher
    if (trigger_now)
        OnChildrenChanged(node_path);
}

void ZkClient::OnChildrenChanged(const std::string& path)
{
    // 获取节点path的所有子节点
    std::vector<std::string> children_vec = GetNodeChildren(path);

    // 调用节点path的变动回调函数，将所有子节点作为参数传入
    {
        std::shared_lock lock(watcher_cb_mutex_);
        if (const auto it = child_watch_callbacks_.find(path); it != child_watch_callbacks_.end()) {
            it->second(path, std::move(children_vec)); // 触发上层业务逻辑
        }
    }
}

std::vector<std::string> ZkClient::GetNodeChildren(const std::string& node_path)
{
    struct ::String_vector children;
    // ZooKeeper 的 watcher 是一次性触发的，触发后必须重新注册。
    const int ret = zoo_wget_children(zhandle_, node_path.c_str(), child_watcher, this, &children);
    if (ret != ZOK) {
        YLOG_WARN("[ZkClient] Failed to get children for node_path: {}, error: {}", node_path, ret);
        return {};
    }

    std::vector<std::string> children_vec;
    for (int i = 0; i < children.count; ++i) {
        children_vec.emplace_back(children.data[i]);
    }
    deallocate_String_vector(&children); // 释放由 ZooKeeper 分配的字符串数组
    return children_vec;
}

void ZkClient::child_watcher(zhandle_t* zh, int type, int state, const char* node_path, void* watcherCtx)
{
    // 在已连接状态下，监听节点的子节点发生变化
    if (type == ZOO_CHILD_EVENT && state == ZOO_CONNECTED_STATE) {
        auto* zk_client = static_cast<ZkClient*>(watcherCtx);
        if (zk_client && node_path) {
            zk_client->OnChildrenChanged(node_path); // 再次获取最新子节点并触发业务回调
        }
    }
}




}
