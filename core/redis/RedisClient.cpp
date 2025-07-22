#include "RedisClient.h"

#include "log.h"

namespace yy::core::redis {

using namespace sw;

void RedisClient::Start(net::EventLoop * loop, size_t pool_size, const std::string& uri) {
    if (!pool_) {
        pool_ = std::make_unique<RedisPool>(loop, uri, pool_size);
    }
}

bool RedisClient::Set(const std::string& key, const std::string& value) {
    const auto conn = pool_->Acquire();
    return conn->conn.set(key, value);

}

bool RedisClient::SetEx(const std::string& key, const std::string& value, const std::chrono::seconds ttl)
{
    const auto conn = pool_->Acquire();
    return conn->conn.set(key, value, ttl);;
}

bool RedisClient::HSet(const std::string& key, const std::string& field, const std::string& value)
{
    const auto conn = pool_->Acquire();
    return conn->conn.hset(key, field, value);
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

bool RedisClient::Del(const std::string& key)
{
    const auto conn = pool_->Acquire();
    return conn->conn.del(key) > 0;
}
}
