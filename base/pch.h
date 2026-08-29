// ============================================================
//  GameServer 预编译头（PCH）
//
//  只放置「稳定 + 被广泛包含」的头文件，让它们只解析一次，避免每个 .cpp
//  都重复解析标准库/protobuf/基础头 —— 这是编译慢的主要开销。
//
//  注意：
//    1. 改动本文件会触发全量重建 PCH（CMake 自动处理），请保持其内容稳定；
//    2. 不要把「正在频繁改动的业务头」放进来，否则会频繁重建 PCH 反而更慢；
//    3. 项目编译期宏（____LINUX / ____DEBUG / THREADED 等）由 CMake 全局统一，
//       与 PCH 一致，因此预编译是安全的。
// ============================================================
#pragma once

// ---- C++ 标准库 ----
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <expected>
#include <format>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

// ---- 第三方（稳定）----
#include <google/protobuf/message.h>

// ---- 项目基础（稳定且被广泛包含）----
#include "Singleton.h"
#include "Timestamp.h"
#include "core_definations.h"
#include "log.h"
#include "net_definations.h"
