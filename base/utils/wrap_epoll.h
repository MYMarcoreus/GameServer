#ifndef ____WRAP_EPOLL_H
#define ____WRAP_EPOLL_H

#include "wrap_fd.h"
#include<sys/epoll.h>
#include<unistd.h>
#include<vector>
#include<stdexcept>
#include<exception>



namespace yy::util {

// 管理某个套接字的事件
class EpollPollerEvent
{
    friend class EpollList;

public:
    explicit EpollPollerEvent(const FileDescriper & fd = FileDescriper::kInvalidFD) : epoll_event_{}
    {
        epoll_event_.data.fd = fd.get_fd();
    }

    // 获取套接字描述符
    [[nodiscard]] int get_fd() const { return epoll_event_.data.fd; }

// 事件管理：
    // 增加一个监听事件
    void add_event(EPOLL_EVENTS EPOLLXXXX) { epoll_event_.events |= EPOLLXXXX; }

    // 删除一个监听事件
    void del_event(EPOLL_EVENTS EPOLLXXXX) { epoll_event_.events &= ~EPOLLXXXX; }

    // 在`epoll()`返回后，使用该函数查看事件是否发生
    [[nodiscard]] bool is_occured(EPOLL_EVENTS EPOLLXXXX) const { return (epoll_event_.events & EPOLLXXXX); }

private:
    struct epoll_event epoll_event_;
};


// Epoll监视列表：管理所有的epoll事件
class EpollList
{
    friend class EpollPollerEvent;

public:
    explicit EpollList(size_t epoll_size = kEpollMaxSize);

    ~EpollList();

    // 返回Pollfd对象
    EpollPollerEvent &operator[](size_t idx);

    [[nodiscard]] size_t size() const { return epoll_arr_.size(); }

    // 增加套接字fd的监视事件EPOLLXXXX，若传入EPOLLET，则自动设置非阻塞
    void add_fd(const FileDescriper & fd, EPOLL_EVENTS EPOLLXXXX);

    // 增加多个套接字fd的监视事件EPOLLXXXX，以初始化列表形式传入，若传入EPOLLET，则自动设置非阻塞
    void add_fd(const FileDescriper & fd, std::initializer_list<EPOLL_EVENTS> EPOLLXXXX);

    //
    void add_fd(EpollPollerEvent & event);

    // 取消监视套接字fd的「所有」监视事件
    void del_fd(const FileDescriper & fd);

    // 取消监视套接字fd的监视事件EPOLLXXXX，
    void del_fd(const FileDescriper & fd, EPOLL_EVENTS EPOLLXXXX);

    // 取消监视套接字fd的监视事件EPOLLXXXX，以初始化列表形式传入
    void del_fd(const FileDescriper & fd, std::initializer_list<EPOLL_EVENTS> EPOLLXXXX);

    void del_fd(EpollPollerEvent & event);

    // 替换监视套接字fd的监视事件为新的event
    void mod_fd(EpollPollerEvent & event);


    // 核心功能：等待事件发生，返回发生的事件个数
    size_t wait();

    size_t wait_timeout(int timeout);

private:
    int epoll_fd_;
    std::vector<EpollPollerEvent> epoll_arr_; // 在epoll_wait中使用，存放发生的事件

    static const int kEpollMaxSize = 16;
};

} // namespace yy


#endif // !____WRAP_EPOLL_H
