#include "AccountRedisDAO.h"
#include "RedisClient.h"


namespace yy::app::account
{
AccountRedisDAO::AccountRedisDAO():
    redis_client_(core::redis::RedisClient::Instance())
{
}

void AccountRedisDAO::Start(net::EventLoop* loop)
{
    redis_client_.Start(loop, 5);
}

bool AccountRedisDAO::SetAccountData(const uint64_t uid, const std::string& username)
{
    const std::string uid_str = std::to_string(uid);
    const auto key = std::format("{}_{}", ACT_field, uid_str);
    return redis_client_.HSet(key, USR_field, username);
}

auto AccountRedisDAO::SetToken(const uint64_t uid, const std::string& token) -> bool
{
    const std::string uid_str = std::to_string(uid);
    const auto key = std::format("{}_{}", USR_TKN_field, uid_str);
    // Token期限为半小时，用户每次发消息时应携带token，服务端会刷新该token的到期时间
    return redis_client_.SetEx(key, token, 1800s);
}

auto AccountRedisDAO::GetToken(const uint64_t uid) const -> std::optional<std::string>
{
    const auto uid_str = std::to_string(uid);
    const auto key = std::format("{}_{}", USR_TKN_field, uid_str);
    return redis_client_.Get(key);
}
}
