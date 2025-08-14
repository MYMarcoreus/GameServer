#pragma once
#include <optional>
#include <string>
#include "account.pb.h"
#include "AccountData.h"
#include "Singleton.h"

namespace yy::net { class EventLoop; }
namespace yy::core::redis { class RedisClient; }

namespace yy::app::logic
{

class LogicRedisDAO final : public Singleton<LogicRedisDAO>{
    SINGLETON_NECESSITY(LogicRedisDAO);
    constexpr static std::string USR_TKN_field = "usr_token";
    constexpr static std::string SCN_TKN_field = "scn_token";

    constexpr static std::string PWD_field = "password";
    constexpr static std::string UID_field = "uid";
    constexpr static std::string USR_field = "username";
    constexpr static std::string ACT_field = "account";
public:
    void Start(net::EventLoop* loop);

    auto GetAndDelSceneToken(uint64_t uid) -> std::optional<std::string>;
    auto GetUserTokenAndRefreshEx(uint64_t uid) const -> std::optional<std::string>;
    auto GetAccountData(uint64_t uid) -> std::optional<account::AccountData>;

private:
    explicit LogicRedisDAO();
    ~LogicRedisDAO() override;

    core::redis::RedisClient& redis_client_;
};

}
