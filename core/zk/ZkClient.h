#pragma once

#include <atomic>
#include <shared_mutex>
#include <string>
#include <functional>
#include <vector>
#include <zookeeper/zookeeper.h>


namespace yy::core::zk
{
// 封装的zk客户端类
class ZkClient
{
    using WatcherCallback = std::function<void(const std::string&, std::vector<std::string>&&)>;
public:
    explicit ZkClient(const std::string & host = {});
    ~ZkClient();
    ///@brief 连接zookeeper
    void Start();

    ///@brief 关闭连接
    void Stop();

    ///@brief 在zkserver上根据指定的 node_path 创建znode节点
    ///@details 例如：node_path = "/root/child" 表示在 "/root" 下创建名为 "child" 的子节点。值为node_data；若父路径 "/root" 不存在，则会创建失败
    void CreateNode(const std::string& node_path, const std::string& node_data = {}, int flags=0);

    ///@brief 获取节点的「数据」
    auto GetNodeData(const std::string& node_path) -> std::string;
    ///@brief 获取节点的「子节点」
    auto GetNodeChildren(const std::string& node_path) -> std::vector<std::string>;

    auto WatchNodeChildren(const std::string& node_path) -> std::vector<std::string>;

    ///@brief 服务调用者：注册并监听 node_path 子节点变化
    void AddChildrenWatcher(const std::string& node_path, WatcherCallback callback);

private:
    // 全局的watcher观察器   zkserver给zkclient的通知
    static void global_watcher(zhandle_t *zh, int type, int state, const char * _path, void *watcherCtx);

    ///@brief 客户端意外断线时，所创建的临时节点会被Zookeeper删除，因此需要恢复这些临时节点
    void RecoverEphemeralNodes();

    ///@brief 子节点变更时 ZooKeeper 线程触发此 watcher
    ///@details 例如监听 "/servers" ：
    /// 1、会触发的情况："/servers" 目录下新增或删除了一个节点 "/servers/server1"时会触发。
    /// 2、不会触发的情况：① "/servers/server1" 节点所存储的数据发生变化
    ///                 ② "/servers/server1" 下添加了新的子节点 "/servers/server1/worker"
    static void child_watcher(zhandle_t* zh, int type, int state, const char* node_path, void* watcherCtx);
    ///@brief 子节点变更的watcher触发时的处理函数：再次注册子节点的watcher + 触发回调
    void OnChildrenChanged(const std::string& path);


private:
    std::string         host_;
    zhandle_t *         zhandle_; // zk的客户端句柄
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
