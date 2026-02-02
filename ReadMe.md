# 基于主从Reactor网络框架的分布式游戏服务器框架

## 简介

基于 C++20 实现异步网络通信与 RPC 框架，采用主从 Reactor 网络模型与非阻塞 I/O，集成 ZooKeeper 服务治理、Redis/MySQL 数据存储与异步日志系统，支持网关接入、账号认证、房间逻辑与多服务协作。

## 分布式架构图

![分布式架构图](./doc/images/system.svg)

## 项目组织结构

- **`/base`（基础组件层）**：实现了常用的数据结构，并实现了强类型配置反序列框架和异步日志库。
  - [`/utils`](./doc/utils.md)：实现了阻塞队列、线性缓冲区、环形缓冲区、基于阻塞队列的线程池、对象池、可继承的单例类等常用数据结构。
  - `/public`：包含了其它开源库的头文件与源文件。
    - md5加解密：https://github.com/xtaci/algorithms/blob/master/include/md5.h 
    - XML解析：https://github.com/leethomason/tinyxml2 
  - [`/config`](./doc/config.md)：实现了强类型XML配置反序列化框架，利用SFINAE与模板特化支持任意类型的序列化/反序列化。
  - [`/log`](./doc/log.md)：实现了**基于C++20`std::format`的异步双缓冲日志库**，支持多日志级别、自定义日志格式与自定义输出目标。
- [**`/net`（网络层）**](./doc/net.md)：实现了基于**主从Reactor网络IO模型**的高性能网络框架。框架支持TCP/UDP通信，提供连接管理与消息收发功能，并通过回调机制为上层应用提供业务扩展接口。
  - 在主从Reactor架构中，主Reactor负责接受新连接，从Reactor负责已建立连接的读写事件处理。每个Reactor线程独立运行事件循环（One Loop per Thread），并结合多路复用技术与非阻塞I/O，实现高性能异步网络通信。
  - 定时器由**`timerfd`+红黑树**（`std::set`）管理；
  - 跨线程事件唤醒与任务投递基于**`eventfd`**实现；
  - 读写缓冲区采用**环形缓冲区**进行管理。 
- **`/core`（核心基础设施层）**：实现了不依赖具体业务的通用组件。
  - [`/zk`](./doc/zk.md)：基于ZooKeeper C API，封装了服务治理客户端组件，支持**服务注册、服务发现、服务节点变更监听以及本地缓存机制**。
  - [`/redis`](./doc/redis.md)：基于redis++实现带连接池的单例Redis客户端，采用RAII自动管理连接，支持自动心跳、重连与常用Redis操作封装。
  - [`/mysql`](./doc/mysql.md)：基于MySQL Connector/C++ X DevAPI实现带连接池的单例MySQL客户端，采用RAII自动管理连接，支持自动心跳与重连。
  - [`/rpc`](./doc/rpc.md)：实现了一套**基于异步通信的 RPC 框架**，使用 Protobuf 定义接口与消息格式，通过ZooKeeper完成服务注册、发现与动态治理，并结合**连接池**机制实现**客户端侧负载均衡**。
  - [`/frontend`](./doc/frontend.md)：基于事件驱动架构的高性能接入服务器，支持TCP与UDP双协议通信，采用Protobuf自定义消息格式。集成连接管理、消息编解码与分发、安全认证、异或加密、心跳检测及客户端UDP端口注册等功能，可通过回调机制与上层业务解耦。
- [**`/game`（业务服务层）**](./doc/game.md)：基于核心基础设施层的通用组件，实现了具体的后端业务。
  - `/gate`：网关服务器。集成接入服务器和异步RPC框架，实现客户端与后端服务的异步消息转发、双向通信与用户广播。结合Redis实现基于Token的消息过滤与续期，并具备连接管理与断线通知功能。
  - `/account`：账号服务器，集成异步RPC框架，实现注册、登录、Token生成（基于跨平台UUID库`stduuid`）与Redis存储防止重复登录。
  - `/center`：中心服务器，集成异步RPC框架，负责大厅房间管理与逻辑服务器负载均衡。
  - `/logic`：逻辑服务器，集成接入服务器与异步RPC框架，采用基于Actor模型的房间架构，实现玩家在场景内的移动、跳跃、进出等状态同步。
  - `/rpc_clients`：统一封装发起跨服务RPC请求的客户端组件。

### 分层架构图

```mermaid
graph BT
    subgraph 业务服务层[业务服务层 /game]
        direction BT
        Gate[网关服务 /gate]
        Account[账号服务 /account]
        Center[中心服务 /center]
        Logic[逻辑服务 /logic]
        RpcClients[RPC客户端 /rpc_clients]
    end

    subgraph 核心设施层[核心基础设施层 /core]
        direction BT
        subgraph ZK[基于Zookeeper的服务发现与注册客户端 /zk]
            direction BT
            ZkServiceClient[服务治理客户端]
            ZkClient[Zookeeper客户端]    
            ZkClient --> ZkServiceClient
        end
        subgraph Redis[Redis客户端 /redis]
            direction BT
            RedisClient[Redis客户端]
            RedisPool[Redis连接池]
            RedisPool --> RedisClient
        end
        subgraph MySQL[MySQL客户端 /redis]
            direction BT
            MySQLClient[MySQL客户端]
            MySQLPool[MySQL连接池]
            MySQLPool --> MySQLClient
        end
        subgraph RPC框架[RPC框架 /rpc]
            RPC_Server[RPC服务器]
            RPC_Client[RPC客户端<br>（模板）]
        end
        subgraph Frontend[接入服务器 /frontend]
            direction BT
        	FrontendServer[接入服务器]
        	ProtobufCodec[Protobuf消息编/解码器]
        	ProtobufDispatcher[Protobuf消息分发器]
        	UserConnection[用户会话<br>TCP+UDP]
        	Protocol[消息协议]
        	ProtobufCodec --> FrontendServer
        	ProtobufDispatcher --> FrontendServer
        	UserConnection --> FrontendServer
        	Protocol --> FrontendServer
        end
    end 

    subgraph 网络层[网络层 /net]
        TcpServer
        TcpConnection
        TcpClient
        UdpServer
        UdpSession
        Timer
        EventLoop
        .......
    end

    subgraph 基础组件层[基础组件层 /base]
        direction BT
        subgraph Utils[常用数据结构 /utils]
            阻塞队列线性缓冲区
            环形缓冲区
            基于阻塞队列的线程池
            对象池
            可继承的单例类
            ......
        end
        Config[强类型XML配置<br>反序列化框架 /config]
        Log[双缓冲异步日志库 /log]
    end

    %% 依赖关系
    ZK --> RPC框架
    核心设施层 --> 业务服务层
    网络层 --> 核心设施层
    基础组件层 --> 网络层
```

