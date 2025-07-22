#pragma once

#include "Singleton.h"
#include "RedisPool.h"
#include <string>
#include <unordered_map>
#include <optional>


namespace yy::core::redis {


class RedisClient final : public Singleton<RedisClient> {
    SINGLETON_NECESSITY(RedisClient)
public:
    // 初始化（只应调用一次）
    void Start(net::EventLoop * loop, size_t pool_size = 5, const std::string& uri = "tcp://127.0.0.1:6379");

    // 封装方法
    bool Set(const std::string& key, const std::string& value);
    bool SetEx(const std::string& key, const std::string& value, std::chrono::seconds ttl);
    bool HSet(const std::string& key, const std::string& field, const std::string& value);
    bool HMSet(const std::string& key, const std::unordered_map<std::string, std::string>& kvs);
    bool Exists(const std::string& key);

    bool HasHashKey(const std::string& key);

    auto Get(const std::string& key) -> std::optional<std::string>;
    auto HGetAll(const std::string& key) -> std::unordered_map<std::string, std::string>;
    auto HGet(const std::string& key, const std::string& field) -> std::optional<std::string>;

    bool Del(const std::string& key);

private:
    std::unique_ptr<RedisPool> pool_ = nullptr;
};

}

