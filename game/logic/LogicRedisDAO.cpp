#include "LogicRedisDAO.h"

#include "RedisClient.h"

namespace yy::app::logic
{

LogicRedisDAO::LogicRedisDAO():
    redis_client_(core::redis::RedisClient::Instance())
{
}

void LogicRedisDAO::Start(net::EventLoop* loop)
{
    redis_client_.Start(loop, 5);
}

auto LogicRedisDAO::GetAndDelSceneToken(const uint64_t uid) -> std::optional<std::string>
{
    const auto key = std::format("{}_{}", SCN_TKN_field, std::to_string(uid));
    auto scene_token = redis_client_.Get(key);
    if (scene_token.has_value()) {
        redis_client_.Del(key);
    }
    return scene_token;
}

auto LogicRedisDAO::GetUserTokenAndRefreshEx(const uint64_t uid) const -> std::optional<std::string>
{
    const auto uid_str = std::to_string(uid);
    const auto key = std::format("{}_{}", USR_TKN_field, uid_str);
    return redis_client_.GetAndRefreshEx(key, 1800s);
}
}
