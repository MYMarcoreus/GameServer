#include "CenterRedisDAO.h"

#include <format>

#include "RedisClient.h"

namespace yy::app::center
{

CenterRedisDAO::CenterRedisDAO():
    redis_client_(core::redis::RedisClient::Instance())
{
}

CenterRedisDAO::~CenterRedisDAO()
{
}

void CenterRedisDAO::Start(net::EventLoop* loop)
{
    redis_client_.Start(loop, 5);
}

auto CenterRedisDAO::SetSceneTokenWithExpire(uint64_t uid, const std::string& token, const std::chrono::seconds ttl) -> bool
{
    const auto key = std::format("{}_{}", SCN_TKN_field, std::to_string(uid));
    // Token期限为半小时，用户每次发消息时应携带token，服务端会刷新该token的到期时间
    return redis_client_.SetEx(key, token, ttl);
}
}
