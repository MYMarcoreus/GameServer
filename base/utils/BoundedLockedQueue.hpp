#ifndef ____BOUNDED_QUEUE_HPP
#define ____BOUNDED_QUEUE_HPP

#include <vector>
#include <atomic>
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>


namespace yy::util {

// muduo实现
template<typename T>
class BoundedLockedQueue
{
public:
    explicit BoundedLockedQueue(int _maxsize = NO_BOUND_SIZE): m_maxsize(_maxsize)  {}

    /// @brief  虽然element参数类型是右值引用&&，但是C++有“引用折叠”机制，就是右值引用&&可以“折叠”以接收左值引用&，
    /// 此时的引用称为“万能引用”，它既能引用左值又能引用右值(常出现在auto和模板这类自动推导类型的场景出现)
    ///
    /// 以int为例，若传入左值引用(如一个局部变量)，想象中的int&&(或int&&&)并不会出现，
    /// 而是会“折叠”成int&以兼容，这个“引用折叠”机制在auto类型推导中也成立：
    /// 如int i;  auto && j = i; 此时j是int &类型而不是int &&。
    ///
    /// 因此万能引用的“引用折叠”机制可以使得我们放心使用右值引用&&而无需再重载一个左值引用&的函数。
    /// 要注意的是，若要防止右值变为左值，在函数中调用别的函数时要是用std::forward()包裹右值参数以防止右值在参数传递时变为左值
    //! 注意，如果Element是一个保存智能指针的类型，element是一个局部变量时，而该变量是一个this指针，则会带来bug
    //! 如传入push(pair<Logger::ptr, LoggMessage::ptr>)时，会产生bug
    template<class Element>
    bool try_push(Element &&element)
    {
        std::lock_guard lock{m_mutex};
        if(!is_bounded() or m_queue.size() < m_maxsize) {
            m_queue.emplace(std::forward<Element>(element)); // 使用std::forward以保留右值引用&&，方式右值变左值
            m_notEmpty.notify_one();
            return true;
        }
        return false;
    }

    template<class Element>
    void wait_push(Element &&element)
    {
        std::unique_lock lock{m_mutex}; // wait需传入unique_lock
        m_notFull.wait(lock, [this]() { return !is_full(); }); // 等到不为满
        m_queue.emplace(std::forward<Element>(element)); // 使用std::forward以保留右值引用&&，方式右值变左值
        m_notEmpty.notify_one(); //
    }

    /// @brief 如果队列为空，则一直等待元素被push，然后取出该元素放入elem中并返回
    void wait_pop(T &element)
    {
        std::unique_lock lock{m_mutex}; // wait需传入unique_lock
        m_notEmpty.wait(lock, [this]() { return !m_queue.empty(); });

        // 注意std::queue::front()返回的是内部元素的「引用」！
        // 而且front()元素也是要pop()的将亡值，便可以使用std::move进行移动
        element = std::move(m_queue.front());
        m_queue.pop();
        m_notFull.notify_one();
    }

    /// @brief 如果队列为空，则一直等待元素被push，然后取出该元素放入智能指针pelem中并返回
    std::shared_ptr<T> wait_pop()
    {
        std::unique_lock lock{m_mutex}; // wait需传入unique_lock
        m_notEmpty.wait(lock, [this]() { return !m_queue.empty(); });
        std::shared_ptr<T> element_ptr{std::make_shared<T>(m_queue.front())};
        m_queue.pop();
        m_notFull.notify_one();
        return element_ptr;
    }

    /// @brief 如果队列为空，返回false，否则，然后取出该元素放入elem中并返回
    bool try_pop(T &element)
    {
        std::lock_guard lock{m_mutex};
        if (m_queue.empty())
            return false;
        element = std::move(m_queue.front());
        m_queue.pop();
        return true;
    }

    /// @brief 如果队列为空，返回false，否则，然后取出该元素放入智能指针pelem中并返回
    std::shared_ptr<T> try_pop()
    {
        std::lock_guard lock{m_mutex};
        if (m_queue.empty())
            return std::shared_ptr<T>();
        std::shared_ptr<T> element_ptr{std::make_shared<T>(m_queue.front())};
        m_queue.pop();
        return element_ptr;
    }

    void swap(BoundedLockedQueue & other)
    {
        std::lock_guard lock{m_mutex};
        std::swap(m_queue, other.m_queue);
    }

    [[nodiscard]] bool empty() const
    {
        std::lock_guard lock{m_mutex};
        return m_queue.empty();
    }

    [[nodiscard]] int size() const
    {
        std::lock_guard lock{m_mutex};
        return m_queue.size();
    }

    [[nodiscard]] int maxsize() const { return m_maxsize; }
    [[nodiscard]] bool is_bounded() const { return m_maxsize != NO_BOUND_SIZE; }
    [[nodiscard]] bool is_full() { return m_queue.size() == m_maxsize; }

    // 禁止复制与移动该容器
    BoundedLockedQueue(const BoundedLockedQueue &) = delete;
    BoundedLockedQueue &operator=(const BoundedLockedQueue &) = delete;
    BoundedLockedQueue(BoundedLockedQueue &&) = delete;
    BoundedLockedQueue &operator=(BoundedLockedQueue &&) = delete;

    ~BoundedLockedQueue() = default;

    static constexpr int            NO_BOUND_SIZE = INT_MAX;
private:
    const int                   m_maxsize;
    std::queue<T>               m_queue;
    std::condition_variable     m_notEmpty;
    std::condition_variable     m_notFull;
    mutable std::mutex          m_mutex;
};



} // yy::util

#endif // ____BOUNDED_QUEUE_HPP
