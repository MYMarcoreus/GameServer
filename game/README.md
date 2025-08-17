## 网关服（GateServer）的时序图

### 带token过滤机制的异步消息转发功能

```mermaid
sequenceDiagram
    autonumber
    participant Client as 客户端
    participant GateServer as GateServer
    participant RpcClient as RpcClient
    participant BackendService as 后端服务

    Client->>GateServer: 发起请求
    activate GateServer
    GateServer->>GateServer: 请求过滤处理：在redis中验证并<br>续期token（登录请求则不进行过滤）
    
    alt 验证通过
        GateServer->>RpcClient: 转发请求
        deactivate GateServer
        
        activate RpcClient
        RpcClient->>BackendService: 轮转法选择一个提供服务的节点，发起RPC请求
        deactivate RpcClient
        
        activate BackendService
        BackendService->>RpcClient: 返回RPC响应
        deactivate BackendService
        
        activate RpcClient
        RpcClient->>GateServer: 执行响应回调
        deactivate RpcClient
        
        activate GateServer
        GateServer->>GateServer: 响应处理（如果是登录响应，<br>则会提取token保存至redis）
        GateServer->>Client: 返回响应
        deactivate GateServer
    else 验证失败
        GateServer->>Client: 拒绝请求，关闭连接
    end
    
    
```

### 用户广播功能

```mermaid
sequenceDiagram
	autonumber
    participant CenterServer as 中心服
    participant GateServer as GateServer
    participant ClientX as 客户端X

    CenterServer->>GateServer: 调用BroadcastRoom RPC
    activate GateServer
    loop 遍历每个目标UID
        GateServer->>GateServer: 使用UID查找映射表，<br>找到对应的用户连接
        GateServer->>GateServer: 创建广播消息<br>(CreateMessage+Parse)
        GateServer->>ClientX   : 发送消息(UID_A)
    end
    deactivate GateServer
    GateServer-->>CenterServer: RPC调用返回
```

## 账号服（AccountServer）的时序图

### 登录时序图

```mermaid
sequenceDiagram
    autonumber
    participant Client
    participant GateServer
    participant AccountServer
    participant MySQL
    participant Redis

    rect rgb(200, 223, 255, 0.20)
    Note over Client, Redis: 登录流程
    Client ->> GateServer: LoginReq(用户名, 密码)
    GateServer ->> AccountServer: RPC LoginReq
    AccountServer ->> MySQL: 查找用户名
    alt 账户存在
        MySQL -->> AccountServer: (uid, 密码)
        AccountServer ->> AccountServer: 验证密码
        alt 密码正确
            AccountServer ->> Redis: 查找uid的user_token
            alt 无现存user_token
                AccountServer ->> AccountServer: 生成128位的user_token
                AccountServer ->> Redis: 存储uid的user_token
                AccountServer -->> GateServer: RPC LoginRsp(success, uid, user_token)
            else 有现存user_token
                AccountServer -->> GateServer: RPC LoginRsp(already_logged_in)
            end
        else 密码错误
            AccountServer -->> GateServer: RPC LoginRsp(password_error)
        end
    else 账户不存在
        AccountServer -->> GateServer: RPC LoginRsp(account_not_exist)
    end
    GateServer -->> Client: LoginRsp
    end
```

### 注册时序图

```mermaid
sequenceDiagram
    autonumber
    participant Client
    participant GateServer
    participant AccountServer
    participant MySQL
    participant Redis

    rect rgb(200, 223, 255, 0.20)
    Note over Client, Redis: 登录流程
    Client ->> GateServer: LoginReq(用户名, 密码)
    GateServer ->> AccountServer: RPC LoginReq
    AccountServer ->> MySQL: 查找用户名
    alt 账户存在
        MySQL -->> AccountServer: (uid, 密码)
        AccountServer ->> AccountServer: 验证密码
        alt 密码正确
            AccountServer ->> Redis: 查找uid的user_token
            alt 无现存user_token
                AccountServer ->> AccountServer: 生成128位的user_token
                AccountServer ->> Redis: 存储uid的user_token
                AccountServer -->> GateServer: RPC LoginRsp(success, uid, user_token)
            else 有现存user_token
                AccountServer -->> GateServer: RPC LoginRsp(already_logged_in)
            end
        else 密码错误
            AccountServer -->> GateServer: RPC LoginRsp(password_error)
        end
    else 账户不存在
        AccountServer -->> GateServer: RPC LoginRsp(account_not_exist)
    end
    GateServer -->> Client: LoginRsp
    end

    rect rgb(200, 255, 230, 0.20)
    Note over Client, Redis: 注册流程
    Client ->> GateServer: RegisterReq(username, password)
    GateServer ->> AccountServer: RPC RegisterReq
    AccountServer ->> MySQL: 查找用户名
    alt 用户名不存在
        AccountServer ->> MySQL: 插入(用户名, 密码)
        MySQL -->> AccountServer: 新uid(基于MySQL的自增键，不严谨)
        AccountServer -->> GateServer: RPC RegisterRsp(success, uid)
    else 用户名已存在
        AccountServer -->> GateServer: RPC RegisterRsp(account_exist)
    end
    GateServer -->> Client: RegisterRsp
    end
```



## 中心服（CenterServer）房间管理的时序图

### 逻辑服信息管理时序图

```mermaid
sequenceDiagram
    autonumber
    participant CenterServer
    participant Redis
    participant LogicServer

  %% ====== 逻辑服务器初始化流程 ======
    rect rgba(200, 220, 255, 0.2)
        Note over CenterServer,LogicServer: 获取逻辑服务器信息(1s一次的心跳)
        loop 遍历每个逻辑服务器的内部RPC地址（从Zookeeper中获取）
            CenterServer->>LogicServer: GetLogicAddrReq
            LogicServer-->>CenterServer: GetLogicAddrRsp (对外IP/端口)
            CenterServer->>CenterServer: 更新逻辑服务器信息
        end
    end
```

### 中心服房间管理时序图

```mermaid
sequenceDiagram
    autonumber
    participant Client
    participant GateServer
    participant CenterServer
    participant LogicServer

    %% ====== 创建房间流程 ======
    rect rgba(200, 200, 215, 0.2)
        Note over Client,LogicServer: 创建房间流程
        Client->>GateServer: CreateRoomReq
        GateServer->>CenterServer: CreateRoomReq
        CenterServer->>LogicServer: 负载均衡：选择房间数最少的服务器，发起NewRoomReq
        LogicServer-->>CenterServer: NewRoomRsp (IP/端口)
        CenterServer->>CenterServer: 记录房间并添加房主
        CenterServer-->>GateServer: CreateRoomRsp
        GateServer-->>Client: CreateRoomRsp (房间信息)
    end

    %% ====== 搜索房间流程 ======
    rect rgba(180, 240, 180, 0.2)
        Note over Client,CenterServer: 搜索房间流程
        Client->>GateServer: SearchRoomReq
        GateServer->>CenterServer: SearchRoomReq
        CenterServer->>CenterServer: 查询所有房间数据
        CenterServer-->>GateServer: SearchRoomRsp (房间列表)
        GateServer-->>Client: SearchRoomRsp
    end
    
    %% ====== 加入房间流程 ======
    rect rgba(200, 255, 220, 0.2)        
        Note over Client,CenterServer: 加入房间流程
        Client->>GateServer: SelfJoinRoomReq
        GateServer->>CenterServer: SelfJoinRoomReq
        CenterServer->>CenterServer: 检查玩家是否已加入
        CenterServer->>CenterServer: 原子操作添加玩家
        CenterServer-->>GateServer: SelfJoinRoomRsp (成功)
        GateServer-->>Client: SelfJoinRoomRsp
        
        CenterServer->>GateServer: BroadcastRoom(OtherJoinRoomRsp)
        GateServer->>Client: OtherJoinRoomRsp (通知新玩家加入)
    end
    
    %% ====== 退出房间流程 ======
    rect rgba(255, 220, 200, 0.2)
        Note over Client,CenterServer: 退出房间流程
        Client->>GateServer: SelfQuitRoomReq
        GateServer->>CenterServer: SelfQuitRoomReq
        CenterServer->>CenterServer: 删除玩家
        CenterServer-->>GateServer: SelfQuitRoomRsp
        GateServer-->>Client: SelfQuitRoomRsp
        
        CenterServer->>GateServer: BroadcastRoom(OtherQuitRoomRsp)
        GateServer->>Client: OtherQuitRoomRsp (通知玩家退出)
    end
    
    %% ====== 用户断线（退出房间）流程 ======
    rect rgba(255, 220, 200, 0.2)
        Note over Client,CenterServer: 用户断线流程
        Client->>GateServer: 连接断开
        GateServer->>CenterServer: UserDisconnectReq
        CenterServer->>CenterServer: 删除玩家
        CenterServer-->>GateServer: UserDisconnectRsp
        CenterServer->>GateServer: BroadcastRoom(OtherQuitRoomRsp)
        GateServer->>Client: OtherQuitRoomRsp (通知玩家退出)
    end

    
    %% ====== 房间清理流程 ======
    rect rgba(200, 230, 255, 0.2)        
        Note over CenterServer,LogicServer: 房间删除流程 (Update)
        loop 遍历每个房间(100ms一次)
            CenterServer->>CenterServer: 尝试删除为空的房间
            alt 删除成功
                CenterServer->>LogicServer: DeleteRoomReq
                LogicServer-->>CenterServer: DeleteRoomRsp(成功)
            end
        end
    end
```

### 逻辑服登录时序图

Redis在这个架构中作为“令牌中心”保存中心服生成的逻辑服登录令牌，供由逻辑服验证客户端。

```mermaid
sequenceDiagram
    autonumber
    participant Client
    participant GateServer
    participant CenterServer
    participant Redis
    participant LogicServer
    
    %% ====== 获取场景令牌流程 ======
    rect rgba(240, 180, 180, 0.2)
        Note over Client,CenterServer: 获取场景令牌（逻辑服登录令牌）流程
        Client->>GateServer: GetEnterSceneTokenReq
        GateServer->>CenterServer: GetEnterSceneTokenReq
        CenterServer->>CenterServer: 生成唯一SceneToken
        CenterServer->>Redis: SetSceneToken(UID,Token,60s)
        CenterServer-->>GateServer: GetEnterSceneTokenRsp
        GateServer-->>Client: GetEnterSceneTokenRsp
    end
    
    %% ====== 令牌验证流程 ======
    rect rgba(180, 180, 240, 0.2)
        Note over Redis,Client: 逻辑服登录（Token验证）
        Client->>LogicServer: EnterSceneReq(SceneToken, UserToken)
        LogicServer->>Redis: 根据UID获取SceneToken和UserToken
        alt Token有效
            Redis-->>LogicServer: Token(有效)
            LogicServer->>Redis: 删除SceneToken并续期UserToken
            LogicServer-->>Client: EnterSceneRsp(成功)
        else Token无效
            Redis-->>LogicServer: Token无效/过期
            LogicServer-->>Client: EnterSceneRsp(失败)
        end
    end

```

## 逻辑服消息时序图

### 逻辑服房间管理时序图

```mermaid
sequenceDiagram
    autonumber
    participant LogicServer
    participant Zookeeper
    participant CenterServer
    participant RoomManager as RoomManager<br>(单线程处理)
    participant Thread as Room<br>(房间所属线程)

    %% 初始化阶段
    rect rgba(200, 220, 255, 0.2)
        Note over LogicServer, CenterServer: 逻辑服启动
        LogicServer->>Zookeeper: RPC服务注册
        Zookeeper-->>CenterServer: 逻辑服RPC服务上线通告
    end

    %% 创建房间流程
    rect rgba(200, 200, 215, 0.2)
        Note over LogicServer, Thread: 创建房间流程
        CenterServer->>LogicServer: NewRoomReq(房间id, 房间名)
        LogicServer->>RoomManager: 创建房间对象
        RoomManager->>Thread: 为房间分配专属事件循环线程
        LogicServer-->>CenterServer: NewRoomRsp(逻辑服IP/端口)
    end

    %% 删除房间流程
    rect rgba(200, 200, 215, 0.2)
        Note over LogicServer, Thread: 删除房间流程
        CenterServer->>LogicServer: DeleteRoomReq(房间id)
        LogicServer->>RoomManager: 删除房间对象
        LogicServer-->>CenterServer: DeleteRoomReq(逻辑服IP/端口)
    end
```



### 逻辑服连接管理与玩家同步时序图

```mermaid
sequenceDiagram
    autonumber
    participant Client
    participant LogicServer
    participant Zookeeper
    participant CenterServer
    participant Redis
    participant RoomManager as RoomManager<br>(单线程处理)
    participant Thread as Room<br>(房间所属线程)

    %% 逻辑服登录流程
    rect rgba(240, 180, 180, 0.2)
        Note over Client, Thread: 逻辑服登录流程
        Client->>LogicServer: TCP连接 (携带SceneToken和UserToken)
        LogicServer->>Redis: 验证SceneToken和UserToken
        Redis-->>LogicServer: 返回Token验证结果
        alt Token验证成功
            LogicServer->>Redis: 获取账户数据
            Redis-->>LogicServer: 返回账户信息
            LogicServer->>RoomManager: 将玩家加入指定房间
            RoomManager->>Thread: 房间添加玩家
            LogicServer-->>Client: 发送SceneLoginRsp(成功)
        else Token验证失败
            LogicServer-->>Client: 发送SceneLoginRsp(失败)
        end
    end

    %% 游戏消息处理
    rect rgba(180, 180, 240, 0.2)
        Client->>LogicServer: 发送游戏消息(玩家移动、跳跃、离开、进入等消息)
        LogicServer->>RoomManager: 根据UID查找房间
        RoomManager->>Thread: 投递消息到房间的消息队列
        Thread->>Thread: 调用消息对应的处理函数
        Thread-->>Client: UDP或TCP广播给房间内的其他玩家
    end

    %% 离线处理
    rect rgba(255, 220, 200, 0.2)
        Note over Client,LogicServer: 客户端断开连接
        Client->>LogicServer: 客户端断开连接
        LogicServer->>RoomManager: 根据UID查找房间
        RoomManager->>Thread: 投递离线事件
        Thread->>Thread: 清理玩家数据
        Thread-->>Client: TCP广播给房间内的其他玩家
        Thread->>Redis: 保存玩家数据(可选)
    end
```

