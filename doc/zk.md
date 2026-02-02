# ZooKeeper服务治理客户端

> 该ZooKeeper服务治理客户端模块**为RPC框架提供服务注册、服务发现与服务变更感知能力，是RPC客户端进行连接管理和负载均衡的基础支撑组件。**

## 类组织结构

- **`ZkClient`**：封装 ZooKeeper C API 的基础客户端，提供：
  - **节点操作**：持久 / 临时节点创建、数据获取、子节点遍历。
  - **监听机制**：子节点变更事件注册。
  - **连接管理**：会话断线重连后自动恢复已注册的临时节点。

- **`ZkServiceClient`**：面向服务治理的高层客户端抽象，基于 `ZkClient` 实现：
  - **服务注册**：以 `IP:Port` 为实例标识创建临时节点，构建 *服务名 → 实例节点* 的树状结构。
  - **服务发现**：支持从 ZooKeeper 拉取服务实例列表，并维护本地缓存以提升访问效率。
  - **服务监听**：封装服务级节点变更监听机制，在服务实例增删时自动更新本地缓存并触发回调。

<img src="//www.plantuml.com/plantuml/png/VLDTInjD6BtVNp6lLxvj0htkbPBI8Yr5zA8G2c7SZDabczrbTopKKb0RRVp8XxHMCJ4sjb91ZMcLg4cqVoOpkrxv5peVYRiDKWXXcJddcVESypmZBeOEdiaOodzOGna4iFLMSTEwQKC7Pe1gGDSTnTZHfsOm6bCjSrAVHU2HejXGJSCK0aDnoqeZP2E7Ll9afKP_jRtI4gdlqTovTtWo1DufYgW2ukb9vBpHLHGpT6HYTW70Gp39tnzsvKt2iIpyFtWCI6w7_5WbQrzwHRzU-kSdv3ehA1CefRiOENrM1EXs1DYM8wZqz80ihKaUgLUxrwktdLptuenkDlsL1M_XyhQpBCeyHLZL9b36mgXVYVExxG9WFJqNFG6wkaoAFHMaqY0xSm7tXQL28mPLRP1FAqSCwtK-aUCjlt1-Nu5Na1K86b2dEDKhhvFIKGzxN99oIN6B-Q0mLZJA_y1iEg4A1nEC4azBDyLDtqLa9crBPu0Cj3ae_7fOc_8-oEQUKBWa_tUL5lO3GthNbYrkoUBfpUQnn8YRc8CWHY-j91gs8TOY80anZ81f0wPScMkPj5iWpX62y0nX_ls4mGI3WwHbidkwqQ3vUXWQqtGZwI1p01xubTlqQmU1oV_YN3QbvNTaGHjD9li4KG4vWiTJ2WPi81zIhlgD3-6KD6Wc3SJj3wRfRYr8SP_kPWKGsNpl76WaJARKvOJmjFhr1hdQO2FiVJxejWet7_FSxlazyZtF1fado7w3g_KW6WqYPNFLpf9IxhLvSxX5VYxyjQUDUV_RegAO5aR0qLCQ1jRquAOocVVkDj-hgqhteZKn7gEr9LfjUjjPijloQdDCscrdM_GdgTAqq04fhdk5XZW8MqdBPp8I2GqFAvjgm7KEgWsjGE1q38oEZm7PNbz9jxFjd-nuzHfhGXpqqcNFj4YkAJ4XwwNBmRiZx6icO_m1" style="zoom: 80%;" />

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
    
    loop 从远端拉取节点信息，更新本地缓存
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
    
    等待事件 --> 子节点监听器触发(ZOO_CHILD_EVENT,<br>ZOO_CONNECTED_STATE): 子节点变更<br>（服务上线/下线）
    子节点监听器触发(ZOO_CHILD_EVENT,<br>ZOO_CONNECTED_STATE) --> 获取最新子节点列表: OnChildrenChanged()触发回调
    获取最新子节点列表 --> 更新本地缓存: 获取最新子节点列表
    更新本地缓存 --> 通知业务层: 执行子节点监听回调
    通知业务层 --> 重新注册: 自动重新注册watcher
    重新注册 --> 等待事件
    
    
    等待事件 --> 全局监听器触发(ZOO_SESSION_EVENT,<br>ZOO_EXPIRED_SESSION_STATE): 会话失效
    全局监听器触发(ZOO_SESSION_EVENT,<br>ZOO_EXPIRED_SESSION_STATE) --> 重启客户端: 自动重连
    重启客户端 --> 重建临时节点: RecoverEphemeralNodes()
    重建临时节点 --> 恢复监听状态: 重新注册所有监听器
    恢复监听状态 --> 等待事件
```









