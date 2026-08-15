# =====================================================================
#  GameServer 多阶段构建镜像
#   builder: 编译环境（gcc-14 + cmake + vcpkg + 全部依赖）
#   runtime: 运行环境（仅可执行文件 + 运行库，不含编译工具链）
#
#  构建:  docker compose -f docker/compose.yaml build
#  运行:  docker compose -f docker/compose.yaml up -d
# =====================================================================

# ---------- 阶段 1：构建 ----------
FROM ubuntu:24.04 AS builder

ARG DEBIAN_FRONTEND=noninteractive
ENV TZ=Asia/Shanghai \
    VCPKG_ROOT=/opt/vcpkg \
    VCPKG_INSTALLED_DIR=/opt/vcpkg_installed

# 1. 更换 apt 镜像源（阿里云）并安装基础工具 + GCC 14
#    （mysql-connector-cpp 8.0.32 与 gcc-15 不兼容，需使用 gcc-14）
RUN sed -i \
        -e 's|http://archive.ubuntu.com/ubuntu/|http://mirrors.aliyun.com/ubuntu/|g' \
        -e 's|http://security.ubuntu.com/ubuntu/|http://mirrors.aliyun.com/ubuntu/|g' \
        /etc/apt/sources.list.d/ubuntu.sources && \
    apt-get update && \
    apt-get install -y --no-install-recommends \
        ca-certificates \
        software-properties-common && \
    add-apt-repository -y ppa:ubuntu-toolchain-r/test && \
    apt-get update && \
    apt-get install -y --no-install-recommends \
        build-essential \
        gcc-14 g++-14 \
        cmake ninja-build \
        git curl zip unzip tar pkg-config \
        libzookeeper-mt-dev && \
    rm -rf /var/lib/apt/lists/*

# 2. 将默认编译器切换到 GCC 14
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 140 \
        --slave /usr/bin/g++ g++ /usr/bin/g++-14 && \
    update-alternatives --set gcc /usr/bin/gcc-14

# 3. 安装 vcpkg（路径由 VCPKG_ROOT 环境变量控制）
#    注意：必须保留 .git 目录 —— manifest 模式的 builtin-baseline 需要
#    git 历史来 checkout 对应版本的端口文件
RUN git clone https://github.com/microsoft/vcpkg.git "${VCPKG_ROOT}" && \
    cd "${VCPKG_ROOT}" && \
    ./bootstrap-vcpkg.sh

# 4. 先只拷贝依赖清单并编译三方依赖（独立缓存层，源码改动不会重装依赖）
WORKDIR /workspace
COPY vcpkg.json ./
RUN "${VCPKG_ROOT}/vcpkg" install --triplet x64-linux

# 5. 拷贝源码并编译
COPY . /workspace
RUN cmake --preset linux-debug && \
    cmake --build --preset debug -j"$(nproc)"

# ---------- 阶段 2：运行 ----------
FROM ubuntu:24.04 AS runtime

# 仅需 zookeeper 运行库；其余 vcpkg 依赖已静态链接进可执行文件，
# libstdc++/libgcc 等由 ubuntu:24.04 自带（GLIBCXX 3.4.32 已满足）
RUN sed -i \
        -e 's|http://archive.ubuntu.com/ubuntu/|http://mirrors.aliyun.com/ubuntu/|g' \
        -e 's|http://security.ubuntu.com/ubuntu/|http://mirrors.aliyun.com/ubuntu/|g' \
        /etc/apt/sources.list.d/ubuntu.sources && \
    apt-get update && \
    apt-get install -y --no-install-recommends \
        libzookeeper-mt2 && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
COPY --from=builder /workspace/build-debug/CenterServer \
                    /workspace/build-debug/LogicServer \
                    /workspace/build-debug/AccountServer \
                    /workspace/build-debug/GateServer \
                    ./build-debug/
COPY --from=builder /workspace/config ./config

# 启动脚本
COPY docker/start.sh /usr/local/bin/start.sh
RUN chmod +x /usr/local/bin/start.sh

CMD ["/usr/local/bin/start.sh"]
