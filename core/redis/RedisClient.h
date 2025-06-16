//
// Created by yy572 on 25-6-10.
//

#ifndef REDISCLIENT_H
#define REDISCLIENT_H

#include "RedisConnectionPool.h"
#include <string>
#include <unordered_map>
#include <optional>


namespace yy::core {


class RedisClient {
public:
    // 获取全局单例
    static RedisClient& Instance() {
        static RedisClient instance;
        return instance;
    }

    // 初始化（只应调用一次）
    void Init(const std::string& uri = "tcp://127.0.0.1:6379", size_t pool_size = 5);

    // Redis 封装方法示例
    void Set(const std::string& key, const std::string& value);
    std::optional<std::string> Get(const std::string& key);
    std::unordered_map<std::string, std::string> HGetAll(const std::string& key);

private:
    RedisClient() = default;
    ~RedisClient() = default;
    RedisClient(const RedisClient&) = delete;
    RedisClient& operator=(const RedisClient&) = delete;

    std::unique_ptr<RedisConnectionPool> pool_;
};

}

#endif //REDISCLIENT_H
