#pragma once
#include <condition_variable>
#include <mutex>
#include <queue>
#include <memory>
#include <chrono>

using namespace std::chrono_literals;



namespace yy::util
{

// 《C++ Concurrency in Action》第二版的相关实现
template<typename T>
class UnboundedLockedQueue
{
private:
    struct Node {
        /* 不存储T，而是存储T的智能指针
         * 原因：
         *      ① 在某种实现中，线程A执行push()，在push()会执行notify_one()，wait_pop会执行wait()，
         *      若wait线程B被notify唤醒，然后将std::queue<T>.front()的元素T移动构造一个新的智能指针，
         *      而智能指针的构造可能抛出异常，若此时智能指针抛出异常，则被唤醒的线程B会结束，因此此时就没有线程被唤醒，线程A也不能得知。
         *      若队列中存储的是元素的智能指针，就可以在push一开始就构造智能指针，然后存入std::queue，再notify，
         *      因为智能指针的拷贝不抛出异常，所以线程B无需构造智能指针，就算构造出了问题，线程A也能得知
        */
        std::shared_ptr<T>       data;
        std::unique_ptr<Node>    next;
    };

    std::mutex            head_mutex;
    std::unique_ptr<Node> head;       // 使用unique_ptr，保证在出队时一定能自动删除所管理的Node对象

    std::mutex            tail_mutex;
    Node *                tail; // 使用虚结点，该虚节点是尾结点

    std::condition_variable data_cv;

    Node * get_tail()
    {
        std::lock_guard tail_lock(tail_mutex);
        return tail;
    }

    std::unique_ptr<Node> pop_head()
    {
        std::unique_ptr<Node> old_head = std::move(head); // 取得首节点（内部有队首数据）
        head = std::move(old_head->next); // 更新首结点指针，head指向首节点的下一节点
        return old_head;
    }


public:
    using value_type = T;

    UnboundedLockedQueue(): head(new Node), tail(head.get()) //, m_size{0}
    {}

    UnboundedLockedQueue(const UnboundedLockedQueue & other) = delete;
    UnboundedLockedQueue & operator=(const UnboundedLockedQueue & other) = delete;

    bool empty()
    {
        std::lock_guard head_lock(head_mutex);
        return (head.get() == get_tail());
    }

    std::shared_ptr<T> try_pop()
    {
        /* 注意：先上head锁，再在get_tail()里面上tail锁
         * 以下的实现是错误的：
         *
         * Node * old_tail = get_tail(); // 问题所在
         * std::lock_guard head_lock(head_mutex);
         * if(head.get() == old_tail)
         *     return nullptr;
         *
         * 在get_tail()返回后，head_mutex上锁前，有一段无锁空闲期，
         * 这一段时间可能导致tail被改动，从而使得old_tail数据过期，使得刚才get_tail()返回的结点不再是尾结点。
         * */
        //! 加锁可能会抛出异常，但在加锁成功后，余下的操作都不会抛出异常，所以异常安全
        std::lock_guard head_lock(head_mutex);
        if(head.get() == get_tail()) // head==tail时，只有一个虚结点，队列为空
            return nullptr;

        /* 到这一步时，队列至少有2个结点，一个有效结点(头)，一个虚节点(尾)，
           现在`head_mutex`已锁，无需关心tail，因为现在tail的改动不会影响head，所以不持有tail_mutex也没事 */
        std::unique_ptr<Node> old_head = pop_head(); // old_head是unique_ptr，在函数退出后，确保该结点会被析构释放
        return old_head ? old_head->data : nullptr ;
    }

    bool try_pop(T & value)
    {
        std::lock_guard head_lock(head_mutex);
        if(head.get() == get_tail())
            return false;

        value = std::move(*head->data);
        auto old_head = pop_head();
        return old_head != nullptr;
    }

    std::shared_ptr<T> wait_pop()
    {
        std::unique_lock head_lock(head_mutex);
        data_cv.wait(head_lock, [&]{ return head.get() != get_tail(); } );
        auto old_head = pop_head();
        return old_head->data; //! 返回被pop的结点的数据
    }

    void wait_pop(T & value)
    {
        std::unique_lock head_lock(head_mutex);
        data_cv.wait(head_lock, [&]{ return head.get() != get_tail(); } );

        value = std::move(*head->data); //! 将要pop结点的数据移入value中返回
        pop_head();
    }

    bool wait_pop_for(T & value, std::chrono::milliseconds interval)
    {
        std::unique_lock head_lock(head_mutex);

        // 使用 wait_for + 谓词版本，返回 true 表示条件满足（有数据）
        const bool has_data = data_cv.wait_for(head_lock, interval, [&]{
            return head.get() != get_tail();
        });

        if (not has_data)
            return false;

        value = std::move(*head->data); // 将数据移入 value
        pop_head(); // 弹出队列头
        return true;
    }


    void push(T new_value)
    {
        //! make_shared可能会抛出异常，但在获取tail_lock之后，不会抛出异常
        std::shared_ptr<T> new_data = std::make_shared<T>(std::move(new_value));

        // 创建空结点（新虚节点），作为之后尾部的虚节点
        std::unique_ptr<Node> p(new Node);
        {
            std::lock_guard tail_lock(tail_mutex);

            // 将数据写入目前的尾结点（虚节点），然后让目前的尾结点指向新的虚节点
            tail->data = new_data;
            Node * const new_tail = p.get();
            tail->next = std::move(p);

            // 更新尾指针
            tail = new_tail;
        }
        data_cv.notify_one();
    }

    void push(std::shared_ptr<T> new_data)
    {
        //! make_shared可能会抛出异常，但在获取tail_lock之后，不会抛出异常

        // 创建空结点（新虚节点），作为之后尾部的虚节点
        std::unique_ptr<Node> p(new Node);
        {
            std::lock_guard tail_lock(tail_mutex);

            // 将数据写入目前的尾结点（虚节点），然后让目前的尾结点指向新的虚节点
            tail->data = new_data;
            Node * const new_tail = p.get();
            tail->next = std::move(p);

            // 更新尾指针
            tail = new_tail;
        }
        data_cv.notify_one();
    }
};

}

