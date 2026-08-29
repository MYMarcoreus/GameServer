#pragma once

#include <string>
#include <system_error>

namespace yy::core::redis {

//! Redis 操作结果错误码：配合 C++23 std::expected<T, std::error_code> 使用。
//! 与 Rust 的 Result<T, E> 对应：调用方可区分“key 不存在”(kNotFound) 与“Redis 通信/执行错误”(kError)。
enum class RedisError : int {
    kOk       = 0,
    kNotFound = 1,  //! key 不存在（如 token 无效/已过期）
    kError    = 2,  //! Redis 通信错误或命令执行失败
};

class RedisErrorCategory final : public std::error_category {
public:
    static const RedisErrorCategory& Instance() {
        static const RedisErrorCategory instance;
        return instance;
    }
    const char* name() const noexcept override { return "redis"; }
    std::string message(int ev) const override {
        switch (static_cast<RedisError>(ev)) {
            case RedisError::kOk:       return "OK";
            case RedisError::kNotFound: return "key not found";
            case RedisError::kError:    return "redis error";
            default:                    return "unknown redis error";
        }
    }
};

inline std::error_code make_error_code(RedisError e) noexcept {
    return { static_cast<int>(e), RedisErrorCategory::Instance() };
}

} // namespace yy::core::redis

//! 使 RedisError 可作为 std::error_code 的“枚举错误类型”，从而 std::unexpected(RedisError::kXxx)
//! 可直接用于构造 std::expected<T, std::error_code> 的错误分支
namespace std {
template <>
struct is_error_code_enum<yy::core::redis::RedisError> : std::true_type {};
}
