#include "RedisConnectionPool.h"

namespace yy::core {

RedisConnectionPool::RedisConnectionPool(const std::string& uri, size_t pool_size)
    : uri_(uri), max_size_(pool_size) {
    for (size_t i = 0; i < max_size_; ++i) {
        pool_.push(std::make_unique<Redis>(uri_));
    }
}

RedisConnectionPool::~RedisConnectionPool() {
    std::lock_guard lock(mutex_);
    while (!pool_.empty()) {
        pool_.pop();  // unique_ptr 自动析构，无需手动 delete
    }
}

RedisConnectionPool::RedisPtr RedisConnectionPool::Acquire() {
    std::unique_lock lock_acquire(mutex_);
    cv_.wait(lock_acquire, [this] { return not pool_.empty(); });

    // 取出 unique_ptr 拥有的连接
    auto conn = std::move(pool_.front());
    pool_.pop();
    lock_acquire.unlock();

    // 创建 shared_ptr，并添加自定义 deleter，在 shared_ptr 析构时将连接归还池中
    auto deleter = [this](Redis* ptr)
    {
        // 智能指针引用计数归零，归还连接到连接池中
        std::unique_lock lock_release(this->mutex_);
        this->pool_.push(std::unique_ptr<Redis>(ptr));
        this->cv_.notify_one();
    };

    return RedisPtr{conn.release(), deleter}; // 将所有权移交给 shared_ptr
}

}
