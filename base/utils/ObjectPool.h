#pragma once
#include <memory>
#include <algorithm>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <stdexcept>

#include "log.h"

namespace yy::util {





template<typename T>
class ObjectPool {
public:
    using Creator = std::function<std::unique_ptr<T>()>;
    using Validator = std::function<bool(const T&)>;

    static ObjectPool& Instance() {
        static ObjectPool instance;
        return instance;
    }

    // 只能调用一次的初始化函数
    static void Init(std::string name, size_t max_size, Creator creator, Validator validator = nullptr) {
        std::call_once(get_once_flag(), [&] {
            auto& inst = Instance();
            inst.name_ = std::move(name);
            inst.max_size_ = max_size;
            if (!creator) {
                creator = [] { return std::make_unique<T>(); }; // 提供默认创建器
            }
            inst.creator_ = std::move(creator);
            inst.validator_ = std::move(validator);
            for (size_t i = 0; i < inst.max_size_; ++i) {
                inst.pool_.emplace(inst.creator_());
            }
            inst.is_initialized_ = true;
        });
    }

    std::shared_ptr<T> Acquire(const std::chrono::milliseconds wait_time) {
        if (!is_initialized_) {
            throw std::runtime_error("ObjectPool is not initialized. Call Init() first.");
        }

        std::unique_lock lock_acquire(mutex_);
        if (!cond_.wait_for(lock_acquire, wait_time, [this] { return !pool_.empty() || is_stop_; })) {
            return nullptr;
        }

        if (is_stop_) {
            return nullptr;
        }

        auto obj = std::move(pool_.front());
        pool_.pop();
        lock_acquire.unlock();

        if (validator_ && !validator_(*obj)) {
            obj = creator_();  // 校验失败重新创建
        }

        auto deleter = [this](T* ptr) {
            std::unique_lock lock_release(mutex_);
            if (!is_stop_) {
                pool_.emplace(std::unique_ptr<T>(ptr));
                cond_.notify_one();
            } else {
                delete ptr;
            }
        };

        return std::shared_ptr<T>{obj.release(), deleter};
    }

    void Shutdown() {
        std::lock_guard lock(mutex_);
        is_stop_ = true;
        while (!pool_.empty()) {
            pool_.pop();
        }
        cond_.notify_all();
    }

private:
    ObjectPool() = default;
    ~ObjectPool() {
        Shutdown();
    }

    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    static std::once_flag& get_once_flag() {
        static std::once_flag flag;
        return flag;
    }

private:
    std::string name_;
    std::queue<std::unique_ptr<T>> pool_;
    Creator creator_;
    Validator validator_;
    size_t max_size_ = 0;
    bool is_initialized_ = false;
    bool is_stop_ = false;

    std::mutex mutex_;
    std::condition_variable cond_;
};




}
