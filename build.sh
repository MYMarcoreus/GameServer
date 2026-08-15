#!/usr/bin/env bash
# ============================================================
#  GameServer 构建脚本
# ============================================================
set -euo pipefail

# ---------- 颜色与输出（非 TTY 时自动禁用颜色） ----------
if [ -t 1 ]; then
    RED=$'\033[0;31m'; GREEN=$'\033[0;32m'; YELLOW=$'\033[1;33m'; CYAN=$'\033[0;36m'; NC=$'\033[0m'
else
    RED=''; GREEN=''; YELLOW=''; CYAN=''; NC=''
fi

info() { printf '%s[信息]%s %s\n' "$CYAN" "$NC" "$*"; }
ok()   { printf '%s[成功]%s %s\n' "$GREEN" "$NC" "$*"; }
warn() { printf '%s[警告]%s %s\n' "$YELLOW" "$NC" "$*" >&2; }
die()  { printf '%s[错误]%s %s\n' "$RED" "$NC" "$*" >&2; exit 1; }

usage() {
    cat <<EOF
用法: $0 [选项]

选项:
  debug | release         构建类型，默认 debug
  clean                   清理构建目录后重新构建
  -j<N>                   并行编译线程数，默认 $(nproc)
  --vcpkg-root=<路径>     指定 vcpkg 安装路径（优先于环境变量）
  -h, --help              显示本帮助

vcpkg 路径优先级: --vcpkg-root 参数 > VCPKG_ROOT 环境变量

示例:
  $0                              # Debug 构建
  $0 release                      # Release 构建
  $0 --vcpkg-root=~/vcpkg         # 指定 vcpkg 路径
  VCPKG_ROOT=~/vcpkg $0           # 用环境变量
EOF
}

# ---------- 初始变量 ----------
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-build-debug}"
BUILD_TYPE="Debug"
JOBS="$(nproc)"
CLEAN=0
VCPKG_ROOT="${VCPKG_ROOT:-}"
VCPKG_SRC="环境变量"

# ---------- 参数解析 ----------
while [ $# -gt 0 ]; do
    case "$1" in
        -h|--help)
            usage; exit 0 ;;
        --vcpkg-root=*)
            VCPKG_ROOT="${1#--vcpkg-root=}"; VCPKG_SRC="参数" ;;
        --vcpkg-root)
            VCPKG_ROOT="${2:-}"; VCPKG_SRC="参数"
            [ -n "$VCPKG_ROOT" ] || die "--vcpkg-root 需要一个路径参数，例如 --vcpkg-root=/path/to/vcpkg"
            shift ;;
        release|Release|RELEASE) BUILD_TYPE="Release"; BUILD_DIR="build-release" ;;
        debug|Debug|DEBUG)       BUILD_TYPE="Debug";   BUILD_DIR="build-debug" ;;
        clean|Clean|CLEAN)       CLEAN=1 ;;
        -j*) JOBS="${1#-j}" ;;
        *)   warn "未知参数: $1"; usage; exit 1 ;;
    esac
    shift
done

# ---------- 解析 vcpkg 路径 ----------
expand_home() {  # 展开路径开头的 ~ 或 ~user
    case "$1" in
        "~")   printf '%s' "$HOME" ;;
        "~/"*) printf '%s' "$HOME/${1#"~/"}" ;;
        "~"*)  printf '%s' "/home/${1#"~"}" ;;
        *)     printf '%s' "$1" ;;
    esac
}
VCPKG_ROOT="$(expand_home "$VCPKG_ROOT")"

if [ -z "$VCPKG_ROOT" ]; then
    die "未提供 vcpkg 路径。请用 --vcpkg-root=<路径> 或环境变量 VCPKG_ROOT 指定（--help 查看用法）"
fi
if [ ! -d "$VCPKG_ROOT" ]; then
    die "目录不存在（来源: $VCPKG_SRC）: $VCPKG_ROOT"
fi

TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
if [ ! -f "$TOOLCHAIN_FILE" ]; then
    die "不是有效的 vcpkg 目录（缺少 scripts/buildsystems/vcpkg.cmake）: $VCPKG_ROOT"
fi

export VCPKG_ROOT
info "vcpkg: $VCPKG_ROOT（来源: $VCPKG_SRC）"

# ---------- 清理 ----------
cd "$PROJECT_DIR"
if [ "$CLEAN" -eq 1 ]; then
    info "清理构建目录: $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi

# ---------- 配置 ----------
info "配置 CMake（类型=$BUILD_TYPE, 目录=$BUILD_DIR）"
cmake -S . -B "$BUILD_DIR" \
    -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

# ---------- 编译 ----------
info "编译（并行 $JOBS 线程）"
START=$(date +%s)
cmake --build "$BUILD_DIR" -- -j"$JOBS"
ELAPSED=$(( $(date +%s) - START ))

ok "构建完成，耗时 ${ELAPSED} 秒"
info "可执行文件: $PROJECT_DIR/$BUILD_DIR/"
ls -1 "$BUILD_DIR"/{LogicServer,GateServer,AccountServer,CenterServer} 2>/dev/null || true
