# GameServer Docker 环境

使用 Docker 一键搭建完整运行环境（编译工具链 + MySQL + Redis + ZooKeeper + 四个游戏服务）。

> 所有 Docker 相关文件集中在 `docker/` 目录，`compose.yaml` 使用 bridge 桥接网络模式，
> 跨平台通用（Linux 原生 Docker、Docker Desktop、WSL2 均支持）。

## 前置条件

- 任意支持 Docker Compose 的环境（Linux 原生 Docker / Docker Desktop / WSL2）

## 方式一：VS Code Dev Container（开发环境，推荐）

项目根目录已提供 `.devcontainer/devcontainer.json`，在 VS Code 中：

1. 安装 **Dev Containers** 扩展
2. 打开项目，点击左下角 **><** 图标 → **Reopen in Container**

VS Code 会自动：
- 构建开发镜像（工具链 + vcpkg + 依赖）
- 启动 MySQL / Redis / ZooKeeper 依赖服务
- 挂载源码并在容器内打开开发环境（clangd、CMake Tools 已预装）
- 打开后自动执行构建

## 方式二：命令行部署运行

```bash
# 在项目根目录执行（compose 文件在 docker/ 下）
cd /path/to/GameServer

# 1. 构建镜像（首次会编译 vcpkg 依赖 + 整个项目，约 15~30 分钟）
docker compose -f docker/compose.yaml build

# 2. 启动全部服务（MySQL / Redis / ZooKeeper / 四个游戏服务）
docker compose -f docker/compose.yaml up -d

# 3. 查看游戏服务日志
docker compose -f docker/compose.yaml logs -f gameserver
```

> 也可以先 `cd docker` 再执行 `docker compose build` / `docker compose up -d`。

## 服务与端口

| 服务 | 端口 | 说明 |
|------|------|------|
| MySQL | 3306 / 33060（容器内） | 33060 为 X Protocol（游戏服务器使用）；宿主机映射为 3307 / 33061 |
| Redis | 6379 | |
| ZooKeeper | 2181 | |
| GateServer | 11111 / 11112 / 11113 | 对外 TCP / UDP / RPC |
| CenterServer | 14441~14443 | |
| LogicServer | 13333 / 14444 / 13334 | |
| AccountServer | 12221~12223 | |

> 说明：MySQL 容器内端口仍为 3306/33060，游戏服务通过服务名 `mysql:33060` 访问；
> 宿主机映射改为 3307/33061，是为了避开宿主机上可能已占用的 3306/33060（如本机已装 MySQL）。
> 其余端口宿主机与容器 1:1 映射。

## 常用命令

```bash
# 停止全部服务
docker compose -f docker/compose.yaml down

# 停止并清除 MySQL 数据卷
docker compose -f docker/compose.yaml down -v

# 仅重建游戏服务镜像
docker compose -f docker/compose.yaml build gameserver

# 进入游戏服务容器
docker compose -f docker/compose.yaml exec gameserver bash
```

## 目录结构

```
docker/
├── Dockerfile          # 构建镜像（工具链 + vcpkg + 编译）
├── compose.yaml        # 服务编排（MySQL/Redis/ZooKeeper/gameserver）
├── config/             # 容器内使用的配置（MySQL/ZooKeeper 地址为服务名）
├── mysql-init.sql      # MySQL 初始化（用户/库/表）
├── start.sh            # 容器启动脚本（按序启动四个服务）
└── README.md           # 本说明
.devcontainer/
└── devcontainer.json   # VS Code Dev Container 配置
.dockerignore           # 构建时排除的目录（需在构建上下文根目录）
```

## 说明

- **编译器**：镜像内使用 GCC 14（`mysql-connector-cpp 8.0.32` 与 gcc-15 不兼容）。
- **vcpkg 路径**：由环境变量 `VCPKG_ROOT` 控制，不再硬编码：
  - 镜像内：`VCPKG_ROOT=/opt/vcpkg`（Dockerfile 中设置）
  - 本机：`.vscode/settings.json` 的 `cmake.configureEnvironment` 默认设为 `/home/yangyue/projects/vcpkg`
  - 命令行：`export VCPKG_ROOT=/path/to/vcpkg` 后执行 `cmake --preset linux-debug`
- **数据库**：首次启动时自动创建用户 `yy`（密码 `0`）、数据库 `gameserver`、表 `account`。
- **网络**：使用 bridge 桥接网络。容器内游戏服务通过服务名访问 MySQL（`mysql:33060`）、
  ZooKeeper（`zookeeper:2181`）、Redis（`tcp://redis:6379`，由环境变量 `REDIS_URI` 指定）。
  容器专用配置在 `docker/config/` 下（MySQL/ZooKeeper 地址为服务名）；
  宿主机原生运行仍使用仓库根目录 `config/` 下的 `127.0.0.1` 配置。
