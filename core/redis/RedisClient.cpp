#include "RedisClient.h"

#include "log.h"
#include "RedisPool.h"

#include <cstdlib>

namespace yy::core::redis {

using namespace sw;

void RedisClient::Start(net::EventLoop * loop, size_t pool_size, const std::string& uri) {
    // 优先使用环境变量 REDIS_URI（例如 Docker bridge 网络下为 tcp://redis:6379），
    // 未设置时使用调用方传入的 uri（默认 tcp://127.0.0.1:6379）
    std::string effective_uri = uri;
    if (const char* env_uri = std::getenv("REDIS_URI"); env_uri && *env_uri) {
        effective_uri = env_uri;
    }

    if (!pool_) {
        pool_ = std::make_unique<RedisPool>(loop, effective_uri, pool_size);
    }
}

bool RedisClient::Set(const std::string& key, const std::string& value) {
    const auto conn = pool_->Acquire();
    return conn->conn.set(key, value);

}

bool RedisClient::SetEx(const std::string& key, const std::string& value, const std::chrono::seconds ttl)
{
    const auto conn = pool_->Acquire();
    return conn->conn.set(key, value, ttl);
}

bool RedisClient::HSet(const std::string& key, const std::string& field, const std::string& value)
{
    const auto conn = pool_->Acquire();
    return conn->conn.hset(key, field, value);
}

bool RedisClient::HSetEx(const std::string& key, const std::string& field, const std::string& value, const std::chrono::seconds ttl)
{
    const auto conn = pool_->Acquire();
    if (not conn->conn.hset(key, field, value))
        return false;

    // 设置过期时间（单位：秒）
    return conn->conn.expire(key, ttl);
}

bool RedisClient::HMSet(const std::string& key, const std::unordered_map<std::string, std::string>& kvs)
{
    const auto conn = pool_->Acquire();
    conn->conn.hmset(key, kvs.begin(), kvs.end());
    return true;
}

bool RedisClient::Exists(const std::string& key)
{
    const auto conn = pool_->Acquire();
    return conn->conn.exists(key);
}

bool RedisClient::Expire(const std::string& key, std::chrono::seconds ttl)
{
    const auto conn = pool_->Acquire();
    return conn->conn.expire(key, ttl);
}

bool RedisClient::HasHashKey(const std::string& key)
{
    const auto conn = pool_->Acquire();
    return conn->conn.exists(key) and conn->conn.type(key) == "hash";
}

std::optional<std::string> RedisClient::Get(const std::string& key) {
    const auto conn = pool_->Acquire();
    if (auto val = conn->conn.get(key))
        return *val;
    return std::nullopt;
}

std::unordered_map<std::string, std::string> RedisClient::HGetAll(const std::string& key) {
    const auto conn = pool_->Acquire();
    std::unordered_map<std::string, std::string> result;
    conn->conn.hgetall(key, std::inserter(result, result.begin()));
    return result;
}

auto RedisClient::HGet(const std::string& key, const std::string& field) -> std::optional<std::string>
{
    const auto conn = pool_->Acquire();
    if (auto val = conn->conn.hget(key, field))
        return *val;
    return std::nullopt;
}

auto RedisClient::GetAndRefreshEx(const std::string& key, const std::chrono::seconds ttl) -> std::optional<std::string>
{
    const auto conn = pool_->Acquire();
    if (auto val = conn->conn.get(key)) {
        // 成功取值后，刷新该 key 的过期时间
        if (conn->conn.expire(key, ttl))
            return *val;
        return std::nullopt;
    }
    return std::nullopt;
}

bool RedisClient::Del(const std::string& key)
{
    const auto conn = pool_->Acquire();
    return conn->conn.del(key) > 0;
}

bool RedisClient::HDel(const std::string& key, const std::string& field)
{
    const auto conn = pool_->Acquire();
    return conn->conn.hdel(key, field) > 0;
}

RedisClient::RedisClient()
{
}

RedisClient::~RedisClient()
{
}
}
