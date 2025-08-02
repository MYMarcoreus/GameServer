#pragma once
#include <optional>
#include <string>
#include "account.pb.h"
#include "AccountData.h"
#include "Singleton.h"

namespace yy::net { class EventLoop; }
namespace yy::core::redis { class RedisClient; }
using namespace yy::protocol::app;

namespace yy::app::account
{

class AccountRedisDAO final : public Singleton<AccountRedisDAO>{
    SINGLETON_NECESSITY(AccountRedisDAO);
    constexpr static std::string PWD_field = "password";
    constexpr static std::string UID_field = "uid";
    constexpr static std::string USR_field = "username";
    constexpr static std::string TKN_field = "usr_token";
    constexpr static std::string ACT_field = "account";
public:
    explicit AccountRedisDAO();
    void Start(net::EventLoop* loop);

    auto GetAccountData(uint64_t uid) -> std::optional<AccountData>;
    auto SetAccountData(uint64_t uid, const std::string& username, const std::string& password) -> bool;
    auto SetToken(uint64_t uid, const std::string& token) -> bool;

private:
    core::redis::RedisClient& redis_client_;
};

}
