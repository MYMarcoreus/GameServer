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
public:
    explicit LogicRedisDAO();
    void Start(net::EventLoop* loop);

    auto GetAndDelSceneToken(uint64_t uid) -> std::optional<std::string>;
    auto GetUserTokenAndRefreshEx(uint64_t uid) const -> std::optional<std::string>;

private:
    core::redis::RedisClient& redis_client_;
};

}
