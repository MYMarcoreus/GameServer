#include "LogicRedisDAO.h"

#include <format>

#include "RedisClient.h"

using namespace std::chrono_literals;

namespace yy::app::logic
{

LogicRedisDAO::LogicRedisDAO():
    redis_client_(core::redis::RedisClient::Instance())
{
}

LogicRedisDAO::~LogicRedisDAO()
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

auto LogicRedisDAO::GetAccountData(const uint64_t uid) -> std::optional<account::AccountData>
{
    const std::string uid_str = std::to_string(uid);
    const auto key = std::format("{}_{}", ACT_field, uid_str);

    if (not redis_client_.HasHashKey(key)) {
        redis_client_.Del(key);
    }

    const auto usr = redis_client_.HGet(key, USR_field);

    if (usr.has_value()) {
        account::AccountData data;
        data.username = usr.value();
        data.uid = uid;
        return data;
    }
    return std::nullopt;
}
}
