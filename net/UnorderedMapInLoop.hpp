#pragma once
#include "EventLoop.h"
#include <functional>
#include <optional>

namespace yy::net
{

template<typename KeyType, typename ValueType>
    requires std::is_same_v<ValueType, std::shared_ptr<typename ValueType::element_type>>
class UnorderedMapInLoop {
public:
    using Callback = std::function<void(ValueType)>;
    using ConstCallback = std::function<void(const ValueType&)>;
    using OptionalCallback = std::function<void(std::optional<ValueType>)>;
    using MapType = std::unordered_map<KeyType, ValueType>;

    explicit UnorderedMapInLoop(EventLoop* loop) : loop_(loop) { }

    // 添加或更新
    void Insert(const KeyType& key, const ValueType& value)
    {
        loop_->RunCallbackInLoop([this, key, value]{
            map_.emplace(key, std::move(value));
        });
    }

    // 移除
    void Erase(const KeyType& key)
    {
        loop_->RunCallbackInLoop([this, key] {
            map_.erase(key);
        });
    }

    // 异步获取副本，调用回调
    void Get(const KeyType& key, OptionalCallback cb)
    {
        loop_->RunCallbackInLoop([this, key, cb = std::move(cb)] {
            auto it = map_.find(key);
            if (it != map_.end()) {
                cb(it->second);
            } else {
                cb(std::nullopt);
            }
        });
    }

    // 遍历全部元素（只读）
    void ForEach(std::function<void(const KeyType&, const ValueType&)> cb)
    {
        loop_->RunCallbackInLoop([this, cb = std::move(cb)] {
            for (const auto& [k, v] : map_) {
                cb(k, v);
            }
        });
    }

    // 清空
    void Clear()
    {
        loop_->RunCallbackInLoop([this] {
            map_.clear();
        });
    }

private:
    EventLoop* loop_;
    MapType map_;
};


}
