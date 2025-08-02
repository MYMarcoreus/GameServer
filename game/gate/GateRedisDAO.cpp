#include "GateRedisDAO.h"
#include "RedisClient.h"

namespace yy::app::gate
{
GateRedisDAO::GateRedisDAO():
    redis_client_(core::redis::RedisClient::Instance())
{
}

void GateRedisDAO::Start(net::EventLoop* loop)
{
    redis_client_.Start(loop, 5);
}

auto GateRedisDAO::GetToken(const uint64_t uid) const -> std::optional<std::string>
{
    const auto uid_str = std::to_string(uid);
    const auto key = std::format("{}_{}", USR_TKN_field, uid_str);
    return redis_client_.HGet(key, USR_TKN_field);
}

auto GateRedisDAO::SetTokenExprieTime(uint64_t uid, std::chrono::seconds) -> bool
{
    const auto uid_str = std::to_string(uid);
    const auto key = std::format("{}_{}", USR_TKN_field, uid_str);
    return redis_client_.Expire(key, 1800s);
}

auto GateRedisDAO::GetTokenAndRefreshEx(const uint64_t uid) const -> std::optional<std::string>
{
    const auto uid_str = std::to_string(uid);
    const auto key = std::format("{}_{}", USR_TKN_field, uid_str);
    return redis_client_.GetAndRefreshEx(key, 1800s);
}
}
