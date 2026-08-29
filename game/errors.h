#pragma once

#include <string>
#include <system_error>

namespace yy::app {

//! 业务层统一错误码：配合 C++23 std::expected<T, std::error_code> 使用（对应 Rust 的 Result<T, E>）。
//! 使用方式：
//!   std::expected<AccountData, std::error_code> r = std::unexpected(GameError::kAccountNotFound);
//!   if (!r) { if (r.error() == GameError::kDbError) { ... } }
enum class GameError : int {
    kOk                = 0,
    kAccountNotFound   = 1,
    kDuplicateUsername = 2,
    kDbError           = 3,
    kInvalidArgument   = 4,
    //! 后续业务错误码按需扩展
};

//! std::error_category 实现，使 GameError 可转换为 std::error_code
class GameErrorCategory final : public std::error_category {
public:
    static const GameErrorCategory& Instance() {
        static const GameErrorCategory instance;
        return instance;
    }
    const char* name() const noexcept override { return "game"; }
    std::string message(int ev) const override {
        switch (static_cast<GameError>(ev)) {
            case GameError::kOk:                return "OK";
            case GameError::kAccountNotFound:   return "account not found";
            case GameError::kDuplicateUsername: return "duplicate username";
            case GameError::kDbError:           return "database error";
            case GameError::kInvalidArgument:   return "invalid argument";
            default:                            return "unknown game error";
        }
    }
};

inline std::error_code make_error_code(GameError e) noexcept {
    return { static_cast<int>(e), GameErrorCategory::Instance() };
}

} // namespace yy::app

//! 使 GameError 可作为 std::error_code 的“枚举错误类型”：
//! 从而 std::unexpected(GameError::kXxx) 可直接用于构造 std::expected<T, std::error_code> 的错误分支
namespace std {
template <>
struct is_error_code_enum<yy::app::GameError> : std::true_type {};
}
