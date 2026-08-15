#!/bin/bash
# =====================================================================
#  容器启动脚本：按顺序启动四个游戏服务器，并保持容器运行
# =====================================================================
set -e

cd /workspace

# 按依赖顺序启动（Center 最先，Gate 最后）
echo "启动 CenterServer ..."
./build-debug/CenterServer &

sleep 1
echo "启动 LogicServer ..."
./build-debug/LogicServer &

sleep 1
echo "启动 AccountServer ..."
./build-debug/AccountServer &

sleep 1
echo "启动 GateServer ..."
./build-debug/GateServer &

echo "四个服务已启动，容器保持运行"
# 保持容器存活（服务日志会输出到容器 stdout，可通过 docker logs 查看）
wait -n || true
