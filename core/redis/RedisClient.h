#pragma once

#include <chrono>
#include <expected>
#include <memory>

#include "Singleton.h"
#include "RedisError.h"
#include <string>
#include <unordered_map>
#include <optional>


namespace yy::net
{
class EventLoop;
}

namespace yy::core::redis {
class RedisPool;

class RedisClient final : public Singleton<RedisClient> {
    SINGLETON_NECESSITY(RedisClient)
public:
    // 初始化（只应调用一次）
    void Start(net::EventLoop * loop, size_t pool_size = 5, const std::string& uri = "tcp://127.0.0.1:6379");

    // 封装方法
    bool Set(const std::string& key, const std::string& value);
    bool SetEx(const std::string& key, const std::string& value, std::chrono::seconds ttl);
    bool HSet(const std::string& key, const std::string& field, const std::string& value);
    bool HSetEx(const std::string& key, const std::string& field, const std::string& value, std::chrono::seconds ttl);
    bool HMSet(const std::string& key, const std::unordered_map<std::string, std::string>& kvs);
    bool Exists(const std::string& key);
    bool Expire(const std::string& key, std::chrono::seconds ttl);

    bool HasHashKey(const std::string& key);

    auto Get(const std::string& key) -> std::optional<std::string>;
    auto HGetAll(const std::string& key) -> std::unordered_map<std::string, std::string>;
    auto HGet(const std::string& key, const std::string& field) -> std::optional<std::string>;
    //! 取 key 并刷新过期时间；错误码区分 kNotFound（key 不存在）与 kError（Redis 通信/执行错误）
    auto GetAndRefreshEx(const std::string& key, std::chrono::seconds expire_seconds) -> std::expected<std::string, std::error_code>;

    bool Del(const std::string& key);
    bool HDel(const std::string& key, const std::string& field);

private:
    RedisClient();
    ~RedisClient() override;

    std::unique_ptr<RedisPool> pool_;
};

}
