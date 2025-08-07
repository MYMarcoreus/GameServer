#pragma once
#include <optional>
#include <string>
#include "AccountData.h"
#include "Singleton.h"

namespace yy::net { class EventLoop; }
namespace yy::core::redis { class RedisClient; }

namespace yy::app::account
{

class AccountRedisDAO final : public Singleton<AccountRedisDAO>{
    SINGLETON_NECESSITY(AccountRedisDAO);
    constexpr static std::string UID_field = "uid";
    constexpr static std::string USR_field = "username";
    constexpr static std::string USR_TKN_field = "usr_token";
    constexpr static std::string ACT_field = "account";
public:
    explicit AccountRedisDAO();
    void Start(net::EventLoop* loop);

    auto SetAccountData(uint64_t uid, const std::string& username) -> bool;
    auto SetToken(uint64_t uid, const std::string& token) -> bool;
    auto GetToken(uint64_t uid) const -> std::optional<std::string>;

private:
    core::redis::RedisClient& redis_client_;
};

}
