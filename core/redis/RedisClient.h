#ifndef REDISCLIENT_H
#define REDISCLIENT_H

#include "Singleton.h"
#include "RedisPool.h"
#include <string>
#include <unordered_map>
#include <optional>
#include "EventLoop.h"


namespace yy::core {


class RedisClient final : public Singleton<RedisClient> {
    SINGLETON_NECESSITY(RedisClient)
public:
    // 初始化（只应调用一次）
    void Start(net::EventLoop * loop, size_t pool_size = 5, const std::string& uri = "tcp://127.0.0.1:6379");

    // 封装方法
    bool Set(const std::string& key, const std::string& value);
    std::optional<std::string> Get(const std::string& key);
    std::unordered_map<std::string, std::string> HGetAll(const std::string& key);

private:
    std::unique_ptr<RedisPool> pool_ = nullptr;
};

}

#endif //REDISCLIENT_H
