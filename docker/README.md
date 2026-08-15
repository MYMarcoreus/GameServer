# GameServer Docker 环境

使用 Docker 一键搭建完整运行环境（编译工具链 + MySQL + Redis + ZooKeeper + 四个游戏服务）。

> 所有 Docker 相关文件集中在 `docker/` 目录，`compose.yaml` 使用 host 网络模式（Linux 原生 Docker），
> Docker Desktop (macOS/Windows) 不支持 host 网络。

## 前置条件

- Linux 环境（含 WSL2）+ Docker + Docker Compose

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
| MySQL | 3306 / 33060 | 33060 为 X Protocol（游戏服务器使用） |
| Redis | 6379 | |
| ZooKeeper | 2181 | |
| GateServer | 11111 / 11112 / 11113 | 对外 TCP / UDP / RPC |
| CenterServer | 14441~14443 | |
| LogicServer | 13333 / 14444 / 13334 | |
| AccountServer | 12221~12223 | |

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
- **网络**：四个游戏服务通过 `127.0.0.1` 访问同机的 MySQL/Redis/ZooKeeper，故使用 host 网络。
