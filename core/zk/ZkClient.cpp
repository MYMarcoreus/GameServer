#include "ZkClient.h"
#include "log.h"
#include "RemoteXmlConfig.h"
#include <semaphore.h>
#include <algorithm>
#include <iostream>

namespace yy::core::zk
{


void ZkClient::global_watcher(zhandle_t* zh, int type, int state, const char* _path, void* watcherCtx)
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

            //! 收集已监听的服务路径（在锁内快照，避免与 AddChildrenWatcher 并发修改时产生数据竞争）
            std::vector<std::string> watched_paths;
            {
                std::shared_lock lock(zk_client->watcher_cb_mutex_);
                for (const auto& [node_path, callback] : zk_client->child_watch_callbacks_) {
                    watched_paths.push_back(node_path);
                }
            }
            for (const auto& node_path : watched_paths) {
                zk_client->OnChildrenChanged(node_path);
            }
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
    //! 已初始化则无需重复初始化
    if (zhandle_ != nullptr) {
        YLOG_DEBUG("[ZkClient] zookeeper_init repeated initialization!");
        return;
    }

    //! 解析主机地址（仅首次）
    if (host_.empty()) {
        std::string ip;
        std::string port;
        for (auto & node : config::g_remote_config->GetValue().m_remote_nodes) {
            if (node.type == "zookeeper") {
                ip = node.ip;
                port = std::to_string(node.port);
            }
        }
        if (ip.empty() && port.empty()) {
            std::cerr << "未配置ZooKeeper的IP地址或端口，结束程序！" << std::endl;
            std::terminate();
        }
        host_ = std::format("{}:{}", ip, port);
    }

    bool connected = false;

    do {
        //! ① 初始化 ZooKeeper 句柄
        zhandle_ = zookeeper_init(host_.c_str(), global_watcher, 30000, nullptr, this, 0);
        if (zhandle_ == nullptr) {
            break;
        }

        //! ② 阻塞直到连接成功（while 防止虚假唤醒：wait 被唤醒后值不一定改变）
        while (!connected_.load(std::memory_order_acquire)) {
            connected_.wait(false, std::memory_order_acquire);
        }
        connected = true;
    } while (false);

    //! 统一出口：记录初始化结果
    if (connected) {
        YLOG_INFO("[ZkClient] zookeeper_init success!");
    } else {
        YLOG_FATAL("[ZkClient] zookeeper_init error!");
        exit(EXIT_FAILURE);
    }
}

void ZkClient::Stop()
{
    //! 无句柄则无需关闭
    if (zhandle_ == nullptr) {
        return;
    }

    int errcode = ZOK;

    do {
        //! ① 先置空句柄与连接状态，再关闭底层句柄，避免并发访问已失效的 zhandle_
        zhandle_t* handle = zhandle_;
        zhandle_ = nullptr;
        connected_.store(false, std::memory_order_release);

        //! ② 关闭句柄，释放资源
        errcode = zookeeper_close(handle);
    } while (false);

    //! 统一出口：记录关闭结果
    if (errcode != ZOK) {
        YLOG_WARN("[ZkClient] zookeeper_close error, code: {}", errcode);
    }
}

void ZkClient::CreateNode(const std::string& node_path, const std::string& node_data, int flags)
{
    if (zhandle_ == nullptr) {
        this->Start();
    }

    int errcode = ZOK;

    do {
        //! ① 判断节点是否已存在，严格区分「存在 / 不存在 / 异常」三种情况
        errcode = zoo_exists(zhandle_, node_path.c_str(), 0, nullptr);
        if (errcode != ZNONODE) {
            if (errcode != ZOK) {
                break; // 检查异常，交由统一出口记录
            }
            //! 节点已存在
            if (!(flags & ZOO_EPHEMERAL)) {
                errcode = ZNODEEXISTS; // 持久节点已存在，跳过
                break;
            }
            //! 旧临时节点（上次进程残留），先删除再重建，避免读到过期数据
            errcode = zoo_delete(zhandle_, node_path.c_str(), -1);
            if (errcode != ZOK && errcode != ZNONODE) {
                break; // 删除失败，交由统一出口记录
            }
        }

        //! ② 创建节点
        char path_buffer[128] {};
        constexpr int bufferlen = sizeof(path_buffer);
        errcode = zoo_create(zhandle_, node_path.c_str(), node_data.c_str(), static_cast<int>(node_data.length()),
                             &ZOO_OPEN_ACL_UNSAFE, flags, path_buffer, bufferlen);
    } while (false);

    //! 统一出口：按结果记录日志并登记临时节点
    if (errcode == ZOK) {
        YLOG_INFO("[ZkClient] znode create success... <node_path: {}, node_data: {}>", node_path, node_data);
        if (flags & ZOO_EPHEMERAL) {
            std::lock_guard lock(ephemeral_nodes_mutex_);
            ephemeral_nodes_.emplace_back(node_path, node_data, flags);
        }
    } else if (errcode == ZNODEEXISTS) {
        YLOG_DEBUG("[ZkClient] persistent znode already exists: {}", node_path);
    } else {
        YLOG_WARN("[ZkClient] znode create failed... <node_path: {}, node_data: {}>, error: {}", node_path, node_data, errcode);
    }
}

void ZkClient::DeleteNode(const std::string& node_path)
{
    //! 无论连接是否存活，都先从临时节点记录中移除，
    //! 避免会话断开后重连时把已注销的节点重新恢复出来
    {
        std::lock_guard lock(ephemeral_nodes_mutex_);
        ephemeral_nodes_.erase(
            std::remove_if(ephemeral_nodes_.begin(), ephemeral_nodes_.end(),
                [&node_path](const EphemeralNodeInfo& info) { return info.path == node_path; }),
            ephemeral_nodes_.end());
    }

    //! 连接已关闭时，临时节点会随会话断开被 ZooKeeper 自动删除，无需显式删除
    if (zhandle_ == nullptr) {
        return;
    }

    int errcode = ZOK;

    do {
        errcode = zoo_delete(zhandle_, node_path.c_str(), -1);
    } while (false);

    //! 统一出口：记录删除结果日志
    if (errcode == ZOK) {
        YLOG_INFO("[ZkClient] znode delete success... <node_path: {}>", node_path);
    } else if (errcode == ZNONODE) {
        YLOG_DEBUG("[ZkClient] znode already gone... <node_path: {}>", node_path);
    } else {
        YLOG_WARN("[ZkClient] znode delete error... <node_path: {}>, error: {}", node_path, errcode);
    }
}


void ZkClient::RecoverEphemeralNodes()
{
    //! 在锁内对临时节点列表做快照，再在锁外执行网络操作，避免长时间持锁阻塞业务线程
    std::vector<EphemeralNodeInfo> nodes_to_recover;
    {
        std::lock_guard lock(ephemeral_nodes_mutex_);
        nodes_to_recover = ephemeral_nodes_;
    }

    for (const auto& [path, data, flags] : nodes_to_recover) {
        int errcode = zoo_exists(zhandle_, path.c_str(), 0, nullptr);
        if (errcode == ZNONODE) {
            char path_buffer[128]{};
            constexpr int buffer_len = sizeof(path_buffer);
            errcode = zoo_create(zhandle_, path.c_str(), data.c_str(), static_cast<int>(data.length()),
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

    std::string result;
    int errcode = ZOK;

    do {
        //! ① 先通过 zoo_exists 获取节点数据的实际长度，再分配足够的缓冲区，避免固定大小截断
        struct Stat stat{};
        errcode = zoo_exists(zhandle_, node_path.c_str(), 0, &stat);
        if (errcode != ZOK) {
            break;
        }

        if (stat.dataLength <= 0) {
            break;
        }

        //! ② 按实际长度分配缓冲区并读取
        std::string buffer(static_cast<size_t>(stat.dataLength), '\0');
        int bufferlen = static_cast<int>(buffer.size());
        //! zoo_get 是线程安全的，但传入的 buffer 等数据结构需要调用方保证线程安全，这里使用栈上局部变量，天然线程安全
        errcode = zoo_get(zhandle_, node_path.c_str(), 0, buffer.data(), &bufferlen, nullptr);
        if (errcode != ZOK) {
            break;
        }

        buffer.resize(static_cast<size_t>(bufferlen));
        result = std::move(buffer);
    } while (false);

    //! 统一出口：记录错误日志后返回结果
    if (errcode != ZOK) {
        YLOG_WARN("[ZkClient] get znode error... node_path: {}, error: {}", node_path, errcode);
    }
    return result;
}




void ZkClient::AddChildrenWatcher(const std::string& node_path, WatcherCallback callback)
{
    {
        std::unique_lock lock(watcher_cb_mutex_);
        child_watch_callbacks_[node_path] = std::move(callback);
    }

    // 设置 watcher
    WatchNodeChildren(node_path);
}

void ZkClient::OnChildrenChanged(const std::string& path)
{
    // 获取节点path的所有子节点，并再次监听其变化
    std::vector<std::string> children_vec = WatchNodeChildren(path);

    // 调用节点path的变动回调函数，将所有子节点作为参数传入
    {
        std::shared_lock lock(watcher_cb_mutex_);
        if (const auto it = child_watch_callbacks_.find(path); it != child_watch_callbacks_.end()) {
            it->second(path, std::move(children_vec)); // 触发上层业务逻辑
        }
    }
}

std::vector<std::string> ZkClient::WatchNodeChildren(const std::string& node_path)
{
    std::vector<std::string> children_vec;
    struct ::String_vector children{};
    int errcode = ZOK;

    do {
        //! ZooKeeper 的 watcher 是一次性触发的，触发后必须重新注册。
        errcode = zoo_wget_children(zhandle_, node_path.c_str(), child_watcher, this, &children);
        if (errcode != ZOK) {
            break;
        }

        children_vec.reserve(children.count);
        for (int i = 0; i < children.count; ++i) {
            children_vec.emplace_back(children.data[i]);
        }
    } while (false);

    //! 统一出口：释放资源并记录错误日志
    deallocate_String_vector(&children);
    if (errcode != ZOK) {
        YLOG_WARN("[ZkClient] Failed to get children for node_path: {}, error: {}", node_path, errcode);
    }
    return children_vec;
}

std::vector<std::string> ZkClient::GetNodeChildren(const std::string& node_path)
{
    std::vector<std::string> children_vec;
    struct ::String_vector children{};
    int errcode = ZOK;

    do {
        errcode = zoo_wget_children(zhandle_, node_path.c_str(), nullptr, this, &children);
        if (errcode != ZOK) {
            break;
        }

        children_vec.reserve(children.count);
        for (int i = 0; i < children.count; ++i) {
            children_vec.emplace_back(children.data[i]);
        }
    } while (false);

    //! 统一出口：释放资源并记录错误日志
    deallocate_String_vector(&children);
    if (errcode != ZOK) {
        YLOG_WARN("[ZkClient] Failed to get children for node_path: {}, error: {}", node_path, errcode);
    }
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
