#pragma once

#include <atomic>
#include <functional>
#include <shared_mutex>
#include <string>
#include <vector>
#include <zookeeper/zookeeper.h>

namespace yy::core::zk
{
// 封装的zk客户端类
class ZkClient
{
    using WatcherCallback = std::function<void(const std::string&, std::vector<std::string>&&)>;
public:
    explicit ZkClient(const std::string & host = "");
    ~ZkClient();
    // zkclient启动连接zkserver
    void Start();
    void Stop();

    ///@brief 服务提供者：在zkserver上根据指定的path创建znode节点
    void CreateNode(const std::string& path, const std::string& data="", int flags=0);

    ///@brief 服务调用者：根据参数指定的znode节点路径，或者znode节点的值
    auto GetNodeVal(const std::string& node_path) -> std::string;

    auto GetNodeChildren(const std::string& path) -> std::vector<std::string>;

    ///@brief 服务调用者：注册并监听 path 子节点变化
    void AddChildrenWatcher(const std::string& path, bool trigger_now, WatcherCallback callback);

private:

    // 全局的watcher观察器   zkserver给zkclient的通知
    static void global_watcher(zhandle_t *zh, int type, int state, const char *path, void *watcherCtx);

    void RecoverEphemeralNodes();

    // 子节点变更时 ZooKeeper 线程触发此 watcher
    static void child_watcher(zhandle_t* zh, int type, int state, const char* path, void* watcherCtx);
    // 真正的处理函数（获取子节点 & 触发回调）
    void OnChildrenChanged(const std::string& path);


    // zk的客户端句柄
    std::string         host_;
    zhandle_t *         zhandle_;
    std::atomic_bool    connected_;

    struct EphemeralNodeInfo {
        std::string path;
        std::string data;
        int         flags; // ZooKeeper 的节点类型，如 ZOO_EPHEMERAL, ZOO_EPHEMERAL | ZOO_SEQUENCE 等
    };
    std::vector<EphemeralNodeInfo> ephemeral_nodes_;

    // 子节点变更事件的业务回调注册表
    std::unordered_map<std::string, WatcherCallback> child_watch_callbacks_;
    std::shared_mutex watcher_cb_mutex_;
};


}
