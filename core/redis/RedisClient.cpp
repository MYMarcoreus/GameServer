#include "RedisClient.h"

namespace yy::core {

using namespace sw::redis;

void RedisClient::Start(net::EventLoop * loop, size_t pool_size, const std::string& uri) {
    if (!pool_) {
        pool_ = std::make_unique<RedisPool>(loop, uri, pool_size);
    }
}

bool RedisClient::Set(const std::string& key, const std::string& value) {
    auto conn = pool_->Acquire();
    return conn->conn.set(key, value);

}

std::optional<std::string> RedisClient::Get(const std::string& key) {
    auto conn = pool_->Acquire();
    auto val = conn->conn.get(key);
    if (val) return *val;
    return std::nullopt;
}

std::unordered_map<std::string, std::string> RedisClient::HGetAll(const std::string& key) {
    const auto conn = pool_->Acquire();
    std::unordered_map<std::string, std::string> result;
    conn->conn.hgetall(key, std::inserter(result, result.begin()));
    return result;
}

}
