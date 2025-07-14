#pragma once
#include "UnboundedLockedQueue.hpp"
#include <memory>
#include <atomic>
#include <algorithm>

namespace yy::util {


/// @brief 线程安全的对象池
template<class ObjectType>
class ObjectPool
{
public:
    using ptr = std::shared_ptr<ObjectPool>;
public:
    /// @brief
    explicit ObjectPool(int maxsize): m_size{0}, m_maxsize{maxsize}
    {
        // 在线程池中初始化maxsize个对象(使用智能指针管理)
        for(int i = 0 ; i < maxsize ; ++i) {
            // m_pool.push( std::make_shared<T>() );
            m_pool.push(ObjectType{} );
        }
    }

    /// @brief 从池中取出对象
    std::shared_ptr<ObjectType> pop()
    {
        std::shared_ptr<ObjectType> obj = m_pool.try_pop();
        // 如果池中有对象，则取出，否则new一个对象
        if(obj) {
            m_size--;
        } else {
            obj = std::make_shared<ObjectType>();
        }
        return obj;
    }

    /// @brief 归还对象到池中
    void push(std::shared_ptr<ObjectType> obj)
    {
        if(m_size < m_maxsize) {
            m_pool.push(obj);
            m_size++;
        }
        else {
            obj.reset();
        }
    }

private:
    std::atomic_size_t       m_size;
    const int                m_maxsize;
    util::UnboundedLockedQueue<ObjectType> m_pool;
};

}
