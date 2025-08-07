#include "RedisPool.h"
#include "log.h"

#include <iostream>
#include "EventLoop.h"


namespace yy::core::redis {

RedisPool::RedisPool(net::EventLoop * loop, const std::string& uri, size_t pool_size)
    : loop_(loop), uri_(uri), max_size_(pool_size) {
    try {
        for (size_t i = 0; i < max_size_; ++i) {
            sw::redis::Redis conn(uri_);
            conn.ping();

            auto currentTime = std::chrono::system_clock::now().time_since_epoch();
            long long timestamp = std::chrono::duration_cast<std::chrono::seconds>(currentTime).count();

            pool_.push(std::make_unique<RedisConnType>(std::move(conn), timestamp));
        }

        loop_->RunEvery(60s, [this]() {
            if (not is_stop_) {
                YLOG_INFO("RedisPool check_connection!");
                check_connection();
            }
        });
    }
    catch (sw::redis::Error & e) {
        YLOG_ERROR("RedisPool Init Error: ", e.what());
    }
}

RedisPool::~RedisPool() {
    std::lock_guard lock(mutex_);
    is_stop_ = true;
    while (!pool_.empty()) {
        pool_.pop();  // unique_ptr 自动析构，无需手动 delete
    }
}

RedisPool::RedisConnPtr RedisPool::Acquire() {
    std::unique_lock lock_acquire(mutex_);
    cond_.wait(lock_acquire, [this] { return not pool_.empty(); });

    // 取出 unique_ptr 拥有的连接
    auto conn = std::move(pool_.front());
    pool_.pop();
    lock_acquire.unlock();

    // 创建 shared_ptr，并添加自定义 deleter，在 shared_ptr 析构时将连接归还池中
    auto deleter = [this](RedisConnType* ptr)
    {
        // 智能指针引用计数归零，归还连接到连接池中
        std::unique_lock lock_release(this->mutex_);
        this->pool_.emplace(std::unique_ptr<RedisConnType>(ptr));
        this->cond_.notify_one();
    };

    return RedisConnPtr{conn.release(), deleter}; // 将所有权移交给 shared_ptr
}

bool RedisPool::reconnect(long long timestamp)
{
    try {
        sw::redis::Redis raw_conn(uri_);
        auto new_conn = std::make_unique<RedisConnection>(std::move(raw_conn), timestamp);
        new_conn->conn.ping();
        {
            std::lock_guard guard(mutex_);
            pool_.push(std::move(new_conn));
        }

        YLOG_INFO("Redis connection reconnect success");
        return true;

    }
    catch (const sw::redis::Error & e) {
        YLOG_ERROR("Redis Reconnect failed, error is ", e.what());
        return false;
    }
}

void RedisPool::check_connection()
{
    // 先读取要处理的最大连接数，
    size_t target_cnt;
    {
        std::lock_guard guard(mutex_);
        target_cnt = pool_.size();
    }

    size_t valid_cnt = 0;
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const long long timestamp = std::chrono::duration_cast<std::chrono::seconds>(now).count();

    while (valid_cnt < target_cnt) {
        //! 每次线程安全地取出一个连接
        std::unique_ptr<RedisConnection> con;
        {
            std::lock_guard lg(mutex_);
            // targetCount不一定准确，可能在处理while循环中pool_被取出或加入连接，因此需要安全判断
            if (pool_.empty()) {
                break;
            }
            con = std::move(pool_.front());
            pool_.pop();
        }

        // 解锁后做检查
        bool is_healthy = true;
        if (timestamp - con->last_oper_time >= 5) {
            try {
                con->conn.ping();
                con->last_oper_time = timestamp;
            }
            catch (const sw::redis::Error & e) {
                YLOG_ERROR("Error keeping redis connection alive: ", e.what());
                is_healthy = false;
                ++_fail_count;
            }

        }

        // 连接有效!
        if (is_healthy)
        {
            std::lock_guard lg(mutex_);
            pool_.push(std::move(con));
            cond_.notify_one();
        }

        ++valid_cnt;
    }

    // 重连那些失效的连接
    while (_fail_count > 0) {
        if (reconnect(timestamp)) {
            --_fail_count;
        }
        else {
            break;
        }
    }
}
}
