#pragma once

#include <sw/redis++/redis++.h>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <string>

namespace yy::net
{
class EventLoop;
}

namespace yy::core::redis {


class RedisConnection {
public:
    RedisConnection(sw::redis::Redis && r, const int64_t lasttime)
        : conn(std::move(r)), last_oper_time(lasttime) {}

    sw::redis::Redis conn;
    int64_t last_oper_time;
};

class RedisPool {
public:
    using RedisConnType = RedisConnection;
    using RedisConnPtr = std::shared_ptr<RedisConnType>; // 对外返回的智能指针

    RedisPool(net::EventLoop * loop, const std::string& uri, size_t pool_size);
    ~RedisPool();

    // 获取 RedisConnType 连接（RAII 封装，自动归还）
    RedisConnPtr Acquire();

private:
    bool reconnect(long long timestamp);
    void check_connection();

    using RawRedisConnPtr = std::unique_ptr<RedisConnType>; // 对内的原生指针

    net::EventLoop * loop_;
    std::queue<RawRedisConnPtr> pool_;
    std::mutex mutex_;
    std::condition_variable cond_;
    std::string uri_;
    size_t max_size_;
    std::atomic<bool> is_stop_;
    std::atomic<int> _fail_count;
};

}
