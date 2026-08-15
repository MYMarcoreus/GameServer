# Linux（WSL2）原生构建与运行指南

> 本文档讲解**不使用 Docker**，直接在 WSL2 的 Linux（Ubuntu 24.04）中完成
> GameServer 的**环境搭建、编译构建与运行**的完整流程。
> 使用 Docker 的方式请参考 [`../docker/README.md`](../docker/README.md)。

## 一、环境概览

| 组件 | 版本要求 | 用途 |
|------|---------|------|
| WSL2 + Ubuntu | 24.04 | 宿主 Linux 环境 |
| GCC | **14.x**（不能用 15） | 编译器 |
| CMake | ≥ 3.22 | 构建系统 |
| Ninja | 任意 | 构建后端 |
| vcpkg | 最新 master | 三方依赖管理 |
| MySQL | 8.0（X Protocol 33060） | 账号数据存储 |
| Redis | 7.x | Token / 会话 |
| ZooKeeper | 3.9.x | RPC 服务治理 |
| libzookeeper-mt-dev | 3.9.x | ZooKeeper C 客户端库（编译期） |

## 二、启用 WSL2 的 systemd

项目运行依赖 MySQL / Redis / ZooKeeper 三个服务，推荐用 systemd 统一管理。

编辑 `/etc/wsl.conf`：

```ini
[boot]
systemd=true
```

保存后在 **Windows 的 PowerShell / CMD** 中执行：

```powershell
wsl --shutdown
```

重新打开 WSL 终端，验证：

```bash
ps -p 1 -o comm=   # 输出 systemd 即成功
```

## 三、安装编译工具链

### 3.1 基础工具 + GCC 14

> **注意**：`mysql-connector-cpp 8.0.32` 与 GCC 15 不兼容（编译报
> `uint64_t does not name a type`），必须使用 GCC 14。

Ubuntu 24.04 默认 GCC 13，需添加 toolchain PPA 安装 14：

```bash
sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
sudo apt update
sudo apt install -y build-essential gcc-14 g++-14
```

将默认编译器切换到 GCC 14：

```bash
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 140 \
    --slave /usr/bin/g++ g++ /usr/bin/g++-14
sudo update-alternatives --set gcc /usr/bin/gcc-14

gcc --version   # 应显示 14.x
```

### 3.2 其他构建工具与系统库

```bash
sudo apt install -y \
    cmake ninja-build git curl zip unzip tar pkg-config \
    libzookeeper-mt-dev
```

- `libzookeeper-mt-dev`：ZooKeeper C 客户端库，编译期链接 `libzookeeper_mt`。
  **ZooKeeper 不走 vcpkg**（vcpkg 的 zookeeper 下载地址 404 且库名不匹配），
  统一使用系统库。

## 四、安装 vcpkg

```bash
git clone https://github.com/microsoft/vcpkg.git ~/projects/vcpkg
cd ~/projects/vcpkg
./bootstrap-vcpkg.sh
```

> **注意**：必须保留 `.git` 目录——项目的 vcpkg manifest 使用 `builtin-baseline`，
> 需要 git 历史来 checkout 对应版本的端口文件。

设置环境变量并写入 `~/.bashrc` 持久化：

```bash
echo 'export VCPKG_ROOT=$HOME/projects/vcpkg' >> ~/.bashrc
source ~/.bashrc
```

## 五、安装并初始化运行时服务

### 5.1 安装

```bash
sudo apt install -y mysql-server redis-server zookeeper zookeeperd
```

### 5.2 启动

```bash
sudo systemctl start mysql
sudo systemctl start redis-server
sudo systemctl start zookeeper

# 可选：开机自启
sudo systemctl enable mysql redis-server zookeeper
```

### 5.3 初始化 MySQL（用户、库、表）

配置文件（`config/configs_*.xml`）中的 MySQL 连接信息为：
`127.0.0.1:33060`，用户名 `yy`，密码 `0`，库 `gameserver`。

```bash
sudo mysql <<'SQL'
CREATE USER IF NOT EXISTS 'yy'@'localhost' IDENTIFIED BY '0';
CREATE USER IF NOT EXISTS 'yy'@'%' IDENTIFIED BY '0';
GRANT ALL PRIVILEGES ON *.* TO 'yy'@'localhost' WITH GRANT OPTION;
GRANT ALL PRIVILEGES ON *.* TO 'yy'@'%' WITH GRANT OPTION;
CREATE DATABASE IF NOT EXISTS gameserver CHARACTER SET utf8mb4;

USE gameserver;
CREATE TABLE IF NOT EXISTS account (
    uid      BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    username VARCHAR(64)     NOT NULL,
    password VARCHAR(255)    NOT NULL,
    PRIMARY KEY (uid),
    UNIQUE KEY uk_username (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

FLUSH PRIVILEGES;
SQL
```

### 5.4 确认 MySQL X Protocol 已启用

游戏服务器通过 **X Protocol（33060）** 访问 MySQL（`mysql-connector-cpp` X DevAPI）。

```bash
ss -tlnp | grep 33060   # 应有 127.0.0.1:33060 监听
```

若没有监听，手动安装插件：

```bash
sudo mysql -e "INSTALL PLUGIN mysqlx SONAME 'mysqlx.so';"
```

## 六、构建项目

在项目根目录执行：

```bash
cd ~/projects/GameServer

./build.sh              # Debug 构建   → build-debug/
./build.sh release      # Release 构建 → build-release/
./build.sh clean        # 清理后重新构建
./build.sh -j8          # 指定并行编译线程数
./build.sh --vcpkg-root=/path/to/vcpkg   # 覆盖 vcpkg 路径
```

`build.sh` 会自动完成：

- 读取 `VCPKG_ROOT`（优先级：`--vcpkg-root` 参数 > `VCPKG_ROOT` 环境变量）
- 调用 CMake + vcpkg 工具链，自动安装三方依赖到 `<build-dir>/vcpkg_installed/`
- 编译四个服务

构建成功后生成四个可执行文件：

```
build-debug/CenterServer   build-debug/LogicServer
build-debug/AccountServer  build-debug/GateServer
```

> 首次构建会下载并编译全部 vcpkg 依赖（protobuf / lua / redis++ /
> mysql-connector-cpp / gtest / stduuid），耗时较长，请耐心等待。

### 手动方式（等价）

```bash
export VCPKG_ROOT=$HOME/projects/vcpkg
cmake --preset linux-debug
cmake --build --preset debug
```

## 七、运行服务

### 启动顺序

四个服务存在依赖关系，必须按顺序启动：

**Center → Logic → Account → Gate**

服务启动后各自将 RPC 节点注册到 ZooKeeper（`/rpc_services/*`），
需等前一个服务注册完成后再启动下一个。

### 后台运行（推荐）

```bash
cd ~/projects/GameServer
./build-debug/CenterServer  &
sleep 1
./build-debug/LogicServer   &
sleep 1
./build-debug/AccountServer &
sleep 1
./build-debug/GateServer    &
```

### 前台运行（便于观察日志）

分别打开四个终端，各自执行：

```bash
cd ~/projects/GameServer && ./build-debug/CenterServer
cd ~/projects/GameServer && ./build-debug/LogicServer
cd ~/projects/GameServer && ./build-debug/AccountServer
cd ~/projects/GameServer && ./build-debug/GateServer
```

### 停止服务

```bash
pkill -f CenterServer
pkill -f LogicServer
pkill -f AccountServer
pkill -f GateServer
```

> 说明：
> - 配置路径由 `ConfigManager::GetProjectRoot()` 基于 `/proc/self/exe` 推导项目根，
>   因此**可从任意目录启动**，日志仍写到项目根的 `logs/` 目录。
> - 日志同时输出到终端（stdout）和 `logs/<服务名>.log`。

## 八、端口速查表

游戏服务端口：

| 服务 | TCP | UDP | RPC |
|------|-----|-----|-----|
| GateServer | 11111 | 11112 | 11113 |
| AccountServer | 12221 | 12222 | 12223 |
| LogicServer | 13333 | 14444 | 13334 |
| CenterServer | 14441 | 14442 | 14443 |

运行时依赖服务端口：

| 服务 | 端口 |
|------|------|
| MySQL（X Protocol） | 33060 |
| Redis | 6379 |
| ZooKeeper | 2181 |

## 九、验证与常见问题

### 验证

- 四个服务启动无报错，终端日志正常输出
- `ss -tlnp` 可见各服务端口已监听
- ZooKeeper 中出现 `/rpc_services/*` 节点

### 常见问题

1. **GCC 15 编译报 `uint64_t does not name a type`**
   → 请切换到 GCC 14（见 3.1 节）。

2. **`build.sh` 报“未提供 vcpkg 路径”**
   → 未设置 `VCPKG_ROOT` 环境变量，或 `--vcpkg-root=` 路径不正确。

3. **vcpkg 报 builtin-baseline 相关错误**
   → vcpkg 目录缺少 `.git`，重新 `git clone` 获取完整仓库。

4. **服务启动后连不上 MySQL**
   → 确认 33060（X Protocol）已监听；确认用户 `yy`/密码 `0`、
     库 `gameserver`、表 `account` 已创建（见 5.3、5.4 节）。

5. **ZooKeeper 连接失败**
   → `systemctl status zookeeper` 确认服务运行、2181 端口监听。

6. **WSL2 中 `systemctl` 不可用**
   → 未启用 systemd，参考第二章配置 `/etc/wsl.conf`。

## 十、与 Docker 方式的对应关系

| 步骤 | 原生（本文档） | Docker |
|------|---------------|--------|
| 编译工具链 | apt 安装 gcc-14 / cmake / ninja | Dockerfile builder 阶段 |
| 三方依赖 | 本地 vcpkg 安装 | 镜像内 vcpkg |
| 运行时服务 | systemd 管理 MySQL/Redis/ZK | compose 编排 |
| 服务启动 | 手动按序启动四个服务 | `start.sh` 自动启动 |
