#ifndef REDISCONNECTIONPOOL_H
#define REDISCONNECTIONPOOL_H

#include <sw/redis++/redis++.h>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <string>

namespace yy::core {


class RedisConnectionPool {
public:
    using Redis = sw::redis::Redis;
    using RedisPtr = std::shared_ptr<Redis>; // 对外返回的智能指针
    using RawRedisPtr = std::unique_ptr<Redis>; // 对内的原生指针

    RedisConnectionPool(const std::string& uri, size_t pool_size);
    ~RedisConnectionPool();

    // 获取 Redis 连接（RAII 封装，自动归还）
    RedisPtr Acquire();

private:

    std::queue<RawRedisPtr> pool_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::string uri_;
    size_t max_size_;
};

}

#endif
