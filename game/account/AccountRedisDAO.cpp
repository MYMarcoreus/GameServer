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

std::optional<AccountData> AccountRedisDAO::GetAccountData(const uint64_t uid)
{
    const std::string uid_str = std::to_string(uid);
    const auto key = std::format("{}_{}", ACT_field, uid_str);

    if (not redis_client_.HasHashKey(key)) {
        redis_client_.Del(key);
    }

    const auto usr = redis_client_.HGet(key, USR_field);
    const auto pwd = redis_client_.HGet(key, PWD_field);

    if (pwd.has_value() and usr.has_value()) {
        AccountData data;
        data.username = usr.value();
        data.password = pwd.value();
        data.uid = uid;
        return data;
    }
    return std::nullopt;
}

bool AccountRedisDAO::SetAccountData(const uint64_t uid, const std::string& username, const std::string& password)
{
    const std::string uid_str = std::to_string(uid);
    return redis_client_.HSet(uid_str, USR_field, username) and
           redis_client_.HSet(uid_str, PWD_field, password);
}

auto AccountRedisDAO::SetToken(const uint64_t uid, const std::string& token) -> bool
{
    const std::string uid_str = std::to_string(uid);
    const auto key = std::format("{}_{}", TKN_field, uid_str);
    // Token期限为半小时，用户每次发消息时应携带token，服务端会刷新该token的到期时间
    return redis_client_.SetEx(key, token, 1800s);
}

}
