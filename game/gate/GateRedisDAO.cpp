#include "GateRedisDAO.h"

#include <format>

#include "RedisClient.h"

using namespace std::chrono_literals;

namespace yy::app::gate
{
GateRedisDAO::GateRedisDAO():
    redis_client_(core::redis::RedisClient::Instance())
{
}

GateRedisDAO::~GateRedisDAO()
{
}

void GateRedisDAO::Start(net::EventLoop* loop)
{
    redis_client_.Start(loop, 5);
}

auto GateRedisDAO::SetAccountData(const uint64_t uid, const std::string& username) -> bool
{
    const std::string uid_str = std::to_string(uid);
    const auto key = std::format("{}_{}", ACT_field, uid_str);
    return redis_client_.HSet(key, USR_field, username);
}

auto GateRedisDAO::GetToken(const uint64_t uid) const -> std::optional<std::string>
{
    const auto uid_str = std::to_string(uid);
    const auto key = std::format("{}_{}", USR_TKN_field, uid_str);
    return redis_client_.Get(key);
}

auto GateRedisDAO::DelToken(const uint64_t uid) const -> bool
{
    const auto uid_str = std::to_string(uid);
    const auto key = std::format("{}_{}", USR_TKN_field, uid_str);
    return redis_client_.Del(key);
}

auto GateRedisDAO::SetTokenExprieTime(const uint64_t uid, std::chrono::seconds) -> bool
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
