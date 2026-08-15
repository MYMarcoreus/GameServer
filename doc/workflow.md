# 核心业务流程与时序

> 本文档用 Mermaid 时序图描述 GameServer 四个服务的**核心业务流程**：
> 登录/注册、建房、进房/退房、场景登录与同步、掉线处理。
> 协议细节（消息格式、命令枚举、消息体定义）见 [`protocol.md`](./protocol.md)。

## 一、服务拓扑

```mermaid
graph TB
    Client["Unity 客户端<br/>(ThirdPersonDemo)"]

    Gate["GateServer<br/>网关"]
    Account["AccountServer<br/>账号"]
    Center["CenterServer<br/>中心"]
    Logic["LogicServer<br/>逻辑"]

    MySQL[("MySQL<br/>gameserver.account")]
    Redis[("Redis")]
    ZK[("ZooKeeper<br/>/rpc_services")]

    Client -->|"TCP 11111 / UDP 11112"| Gate
    Client -->|"TCP/UDP 直连<br/>(room_ip:room_port)"| Logic

    Gate -->|"RPC"| Account
    Gate -->|"RPC"| Center
    Center -->|"RPC"| Logic

    Account --> MySQL
    Account --> Redis
    Center --> Redis
    Gate --> Redis
    Logic --> Redis

    Gate -.->|"注册/发现"| ZK
    Account -.->|"注册/发现"| ZK
    Center -.->|"注册/发现"| ZK
    Logic -.->|"注册/发现"| ZK
```

| 服务 | TCP 前端 | UDP 前端 | RPC 端口 | ZooKeeper 服务名 |
|------|---------|---------|---------|-----------------|
| GateServer | 11111 | 11112 | 11113 | `GateRoomServiceRpc` |
| AccountServer | — | — | 12223 | `AccountServiceRpc` |
| CenterServer | — | — | 14443 | `CenterRoomServiceRpc` |
| LogicServer | 随机 | 随机 | 随机 | `LogicRoomServiceRpc` |

> LogicServer 的 TCP/UDP/RPC 端口均为**随机分配**（代码绑定 `0.0.0.0`，不读配置），
> 客户端通过建房/进房响应中的 `room_ip`/`room_port` 得知其地址。

## 二、Redis Key 速查

| Key | 类型 | TTL | 说明 |
|-----|------|-----|------|
| `usr_token_{uid}` | string | 1800s（每次校验刷新） | 登录 token，防重复登录 |
| `scn_token_{uid}` | string | 60s（**一次性**，GET 后 DEL） | 场景登录 token |
| `account_{uid}` | hash（`username`） | 无 | 账号缓存（Gate 登录成功时写入） |

- Token 生成：`stduuid` 的 UUID 随机生成器（`util::GenerateToken`）。
- 房间 ID：UUID 经 `std::hash` 转为 `uint64_t`。

## 三、登录流程

```mermaid
sequenceDiagram
    participant C as 客户端
    participant G as GateServer
    participant A as AccountServer
    participant M as MySQL
    participant R as Redis

    Note over C,G: 已完成 TCP 连接 + 异或码/MD5 握手
    C->>G: LoginReq{username, password}
    G->>A: RPC AccountServiceRpc.Login
    A->>M: SELECT uid,password FROM account WHERE username=?
    alt 账号不存在
        A-->>G: LoginRsp{eAccountNotExist}
    else 密码错误
        A-->>G: LoginRsp{ePasswordError}
    else 校验通过
        A->>R: GET usr_token_{uid}
        alt 已存在（防重复登录）
            A-->>G: LoginRsp{eAlreadyLoggedIn}
        else
            A->>A: 生成 token（UUID）
            A->>R: SETEX usr_token_{uid} = token, 1800s
            A-->>G: LoginRsp{eSuccess, account_data, token}
        end
    end
    G-->>C: LoginRsp
    Note over G: 成功后：记录 uid→连接映射、写 account_{uid} 哈希
```

## 四、注册流程

```mermaid
sequenceDiagram
    participant C as 客户端
    participant G as GateServer
    participant A as AccountServer
    participant M as MySQL

    C->>G: RegisterReq{username, password}
    G->>A: RPC AccountServiceRpc.Register
    A->>M: 查询 username 是否已存在
    alt 已存在
        A-->>G: RegisterRsp{eAccountAlreadyExist}
    else 不存在
        A->>M: INSERT INTO account(username, password)
        A-->>G: RegisterRsp{eSuccess, uid}
    end
    G-->>C: RegisterRsp
    Note over C: 注册成功不发放 token，需再走登录流程
```

## 五、建房流程（CreateRoom）

```mermaid
sequenceDiagram
    participant C as 客户端
    participant G as GateServer
    participant CT as CenterServer
    participant L as LogicServer
    participant R as Redis
    participant ZK as ZooKeeper

    C->>G: CreateRoomReq{owner_data, user_token, name, capacity}
    G->>R: GET usr_token_{uid} 校验 user_token
    G->>CT: RPC CenterRoomServiceRpc.CreateRoom
    CT->>ZK: 发现 /rpc_services/LogicRoomServiceRpc
    CT->>CT: 选 room_cnt 最小的逻辑服（负载均衡）
    alt 无可用逻辑服
        CT-->>G: CreateRoomRsp{eNoServer}
    else 选中逻辑服
        CT->>L: RPC LogicRoomServiceRpc.NewRoom(room_data)
        L->>L: RoomManager::AddRoom（创建 Room，启动 Tick）
        L-->>CT: NewRoomRsp{success, room_id, ip, port}
        CT->>CT: RoomInfoController::AddRoom（登记房间与创建者）
        CT-->>G: CreateRoomRsp{eSuccess, room_data, room_ip, room_port}
    end
    G-->>C: CreateRoomRsp
    Note over C: room_ip/room_port 即逻辑服 TCP 地址
```

## 六、查房 / 进房 / 退房

### 6.1 查询房间列表（SearchRoom）

```mermaid
sequenceDiagram
    participant C as 客户端
    participant G as GateServer
    participant CT as CenterServer

    C->>G: SearchRoomReq{uid, user_token}
    G->>CT: RPC CenterRoomServiceRpc.SearchRoom
    CT->>CT: RoomInfoController::GetAllRoomData()
    CT-->>G: SearchRoomRsp{room_datas（不含地址）}
    G-->>C: SearchRoomRsp
```

### 6.2 加入房间（SelfJoinRoom）

```mermaid
sequenceDiagram
    participant C as 客户端
    participant G as GateServer
    participant CT as CenterServer

    C->>G: SelfJoinRoomReq{joinner_data, user_token, room_id}
    G->>CT: RPC CenterRoomServiceRpc.SelfJoinRoom
    CT->>CT: RoomInfoController::AddPlayer（校验存在/满员/已加入）
    alt 失败
        CT-->>G: SelfJoinRoomRsp{eRoomNotExist/eRoomFull/eAlreadyJoined}
    else 成功
        CT->>CT: 更新 RoomInfo.exist_player_datas
        CT-->>G: SelfJoinRoomRsp{eSuccess, room_data, room_ip, room_port}
        CT->>G: RPC GateRoomServiceRpc.BroadcastRoom(OtherJoinRoomRsp)
        G-->>C: 向房间其他玩家广播 OtherJoinRoomRsp
    end
    G-->>C: SelfJoinRoomRsp
```

### 6.3 退出房间（SelfQuitRoom）

```mermaid
sequenceDiagram
    participant C as 客户端
    participant G as GateServer
    participant CT as CenterServer

    C->>G: SelfQuitRoomReq{uid, user_token, room_id}
    G->>CT: RPC CenterRoomServiceRpc.SelfQuitRoom
    CT->>CT: RoomInfoController::DelPlayer
    CT-->>G: SelfQuitRoomRsp{eSuccess}
    CT->>G: RPC GateRoomServiceRpc.BroadcastRoom(OtherQuitRoomRsp)
    G-->>C: 向房间其他玩家广播 OtherQuitRoomRsp
    G-->>C: SelfQuitRoomRsp
```

> 注意：进房/退房只更新 **CenterServer** 的房间信息，逻辑服侧要等场景登录才真正 `AddPlayer`。

## 七、场景登录（客户端直连逻辑服）

```mermaid
sequenceDiagram
    participant C as 客户端
    participant G as GateServer
    participant CT as CenterServer
    participant L as LogicServer
    participant R as Redis

    Note over C,CT: 第一步：请求一次性场景 Token
    C->>G: GetEnterSceneTokenReq{uid, user_token, room_id}
    G->>CT: RPC CenterRoomServiceRpc.GetEnterSceneToken
    CT->>CT: 生成 scene_token（UUID）
    CT->>R: SETEX scn_token_{uid} = scene_token, 60s
    CT-->>G: GetEnterSceneTokenRsp{scene_token}
    G-->>C: GetEnterSceneTokenRsp

    Note over C,L: 第二步：直连逻辑服并登录场景
    C->>L: TCP 连接 + 异或码/MD5 握手
    L-->>C: SecurityCheckRsp{server_udp_port, session_id}
    C->>L: SceneLoginReq{uid, room_id, user_token, scene_token}
    L->>R: GET + DEL scn_token_{uid}（一次性校验）
    L->>R: GET usr_token_{uid} 并刷新 1800s
    alt token 校验失败
        L-->>C: SceneLoginRsp{is_ok=false}
    else 成功
        L->>R: HGET account_{uid} username
        L->>L: RoomManager::AddPlayerToRoom → Room::AddPlayer
        L-->>C: SceneLoginRsp{is_ok=true}
    end
```

## 八、进入场景与状态同步

```mermaid
sequenceDiagram
    participant C1 as 玩家A
    participant L as LogicServer
    participant C2 as 玩家B

    C1->>L: C2SEnterScene{uid, room_id}
    L-->>C1: S2CEnterScene{result, self_data, other_datas}
    L-->>C2: S2COtherPlayerData（新玩家入场通知）

    Note over C1,L: 移动同步（UDP）
    C1->>L: C2SMove{uid, room_id, movement}（UDP）
    L->>L: 更新玩家A的 movement
    L-->>C1: 回显 C2SMove（UDP）
    L-->>C2: S2CMove（UDP 广播给房间其他人）

    Note over C1,L: 跳跃/重力同步（TCP）
    C1->>L: C2SJumpAndGravity（TCP）
    L-->>C1: 回显（TCP）
    L-->>C2: S2CJumpAndGravity（TCP 广播）

    C1->>L: C2SLeaveScene{uid, room_id}
    L-->>C2: S2CLeaveScene（广播）
    L-->>C1: 关闭连接
```

> ⚠️ 通道不对称：**移动走 UDP、跳跃走 TCP**；主动离开广播走 TCP，断线广播走 UDP。

## 九、掉线处理

```mermaid
sequenceDiagram
    participant C as 客户端
    participant G as GateServer
    participant R as Redis
    participant CT as CenterServer

    C--xG: TCP 连接断开
    G->>R: DEL usr_token_{uid}
    G->>G: 移除 uid→连接映射
    G->>CT: RPC UserDisconnectReq
    CT->>CT: RoomInfoController::DelPlayer
    CT->>G: RPC BroadcastRoom(OtherQuitRoomRsp)
    G-->>C: 向房间其他玩家广播退出消息
```

## 十、实现备注与未完成点

| 事项 | 说明 |
|------|------|
| 防重复登录策略 | 登录时若 `usr_token_{uid}` 已存在则**拒绝新登录**（非踢掉旧登录） |
| `Room::Update()` | 当前为空函数，房间 Tick 暂无业务逻辑 |
| `GetEnterSceneToken` | 未校验玩家是否真的在房间内，恒返回 `eSuccess` |
| `SearchRoom` 过滤 | proto 中有 `//todo 过滤条件`，未实现 |
| 端口 | LogicServer 三端口随机分配，配置 `rpcPort` 对逻辑服未生效 |
| 账号哈希写入 | `account_{uid}` 由 **Gate 在登录成功时**写入（Account 的 DAO 是死代码） |
