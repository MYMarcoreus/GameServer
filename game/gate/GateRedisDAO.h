#pragma once
#include <expected>
#include <optional>
#include <string>
#include <cstdint>
#include <chrono>
#include "Singleton.h"
#include "RedisError.h"

namespace yy::net { class EventLoop; }
namespace yy::core::redis { class RedisClient; }

namespace yy::app::gate
{

class GateRedisDAO final : public Singleton<GateRedisDAO>{
    SINGLETON_NECESSITY(GateRedisDAO);
    constexpr static std::string PWD_field = "password";
    constexpr static std::string UID_field = "uid";
    constexpr static std::string USR_field = "username";
    constexpr static std::string USR_TKN_field = "usr_token";
    constexpr static std::string ACT_field = "account";
public:
    void Start(net::EventLoop* loop);

    auto SetAccountData(uint64_t uid, const std::string& username) -> bool;
    auto GetToken(uint64_t uid) const -> std::optional<std::string>;
    auto DelToken(uint64_t uid) const -> bool;
    auto SetTokenExprieTime(uint64_t uid, std::chrono::seconds) -> bool;
    //! 取用户 token 并刷新过期时间；错误码区分 kNotFound（token 不存在）与 kError（Redis 错误）
    auto GetTokenAndRefreshEx(uint64_t uid) const -> std::expected<std::string, std::error_code>;

private:
    explicit GateRedisDAO();
    ~GateRedisDAO() override;
    core::redis::RedisClient& redis_client_;
};

}
