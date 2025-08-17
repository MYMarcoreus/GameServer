# ZooKeeper服务注册与发现系统

## 文件组织结构

- **`ZkClient.h`**：封装ZooKeeper C API的基础客户端，提供：
  - **节点操作**：持久/临时节点创建、数据获取、子节点遍历。
  - **监听机制**：子节点变更事件注册。
  - **连接管理**：会话断线重建时自动恢复临时节点。
- **`ZkServiceClient.h`**：面向微服务的抽象层，实现：
  - **服务注册**：以`IP:Port`为实例名创建临时节点（服务名→实例名的树状结构）。
  - **服务发现**：支持远程强一致获取和本地缓存。
  - **服务监听**：基于`ZkClient`监听能力封装服务级变更回调。

## 服务注册时序图

```mermaid
sequenceDiagram
    participant Provider as AccountServiceRpc提供者
    participant ZkSC as ZkServiceClient
    participant ZkC as ZkClient
    participant ZK as ZooKeeper
    
    Provider->>ZkSC: Register("AccountServiceRpc",<br> "192.168.1.100", "45454")
    ZkSC->>ZkC: CreateNode("/services/AccountServiceRpc")
    ZkC->>ZK: 创建持久节点
    ZK-->>ZkC: 节点创建成功
    ZkSC->>ZkC: CreateNode("/services/AccountServiceRpc/192.168.1.100:45454", ZOO_EPHEMERAL)
    ZkC->>ZK: 创建临时节点（带会话绑定）
    ZK-->>ZkC: 节点创建成功
    ZkC-->>ZkSC: 返回成功
    ZkSC-->>Provider: 注册成功
    
    loop 本地缓存更新
        ZkSC->>ZkC: GetNodeChildren("/services/AccountServiceRpc")
        ZkC->>ZK: 获取子节点列表
        ZK-->>ZkC: 返回["192.168.1.100:45454"]
        ZkC-->>ZkSC: 返回节点列表
        ZkSC->>ZkSC: 更新本地缓存的服务<br>与节点地址映射表
    end
```

## 服务发现时序图

```mermaid
sequenceDiagram
    participant Consumer as AccountServiceRpc服务消费者
    participant ZkSC as ZkServiceClient
    participant ZkC as ZkClient
    participant ZK as ZooKeeper
    
    Consumer->>ZkSC: FetchRemote("AccountServiceRpc")
    ZkSC->>ZkC: GetNodeChildren("/rpc_services/AccountServiceRpc")
    ZkC->>ZK: 获取子节点列表
    ZK-->>ZkC: 返回["192.168.1.100:45454","192.168.1.101:45454"]
    ZkC->>ZkSC: 返回节点列表
    
    loop 解析节点列表中的所有节点
        ZkSC->>ZkC: GetNodeData("/rpc_services/AccountServiceRpc/" + 服务实例名)
        ZkC->>ZK: 获取节点数据
        ZK-->>ZkC: 服务实例地址
        ZkSC->>ZkSC: 解析为IPAddress对象
    end
    
    ZkSC->>ZkSC: 更新本地缓存的服务<br>与节点地址映射表
    ZkSC->>Consumer: 返回[IPAddress1, IPAddress2]
```

## 服务监听机制

```mermaid
stateDiagram-v2
    [*] --> 初始状态
    初始状态 --> 注册监听器: 调用Watch()，监听服务节点"AccountServiceRpc"
    注册监听器 --> 等待事件: 注册全局监听器和子节点监听器
    
    等待事件 --> 子节点监听器触发
    子节点监听器触发 --> 服务上线/下线<br>（子节点变更）: ZOO_CHILD_EVENT
    服务上线/下线<br>（子节点变更） --> 触发回调: OnChildrenChanged()
    触发回调 --> 更新本地缓存: 获取最新节点列表
    更新本地缓存 --> 通知业务层: 执行子节点监听回调
    通知业务层 --> 重新注册: 自动重新注册watcher
    重新注册 --> 等待事件
    
    等待事件 --> 全局监听器触发
    全局监听器触发 --> 会话中断: ZOO_EXPIRED_SESSION
    会话中断 --> 重启客户端: 自动重连
    重启客户端 --> 重建临时节点: RecoverEphemeralNodes()
    重建临时节点 --> 恢复监听状态: 重新注册所有监听器
    恢复监听状态 --> 等待事件
```

