#ifndef LINUXGAMESERVER_POLLEREVENT_H
#define LINUXGAMESERVER_POLLEREVENT_H

#ifdef ____LINUX
#include <sys/epoll.h>
#include <poll.h>

static_assert(EPOLLIN    == POLLIN,    "epoll uses same flag values as poll");
static_assert(EPOLLPRI   == POLLPRI,   "epoll uses same flag values as poll");
static_assert(EPOLLOUT   == POLLOUT,   "epoll uses same flag values as poll");
static_assert(EPOLLERR   == POLLERR,   "epoll uses same flag values as poll");
static_assert(EPOLLHUP   == POLLHUP,   "epoll uses same flag values as poll");
static_assert(EPOLLRDNORM   == POLLRDNORM,   "epoll uses same flag values as poll");
static_assert(EPOLLRDBAND   == POLLRDBAND,   "epoll uses same flag values as poll");
static_assert(EPOLLWRNORM   == POLLWRNORM,   "epoll uses same flag values as poll");
static_assert(EPOLLWRBAND   == POLLWRBAND,   "epoll uses same flag values as poll");
#endif

#ifdef ____WINDOWS
#include <winsock2.h>
#endif

#include <stdint.h>


namespace yy::net {

// 集Epoll和Poll的事件的共同点
struct PollerEvent final
{
public:
    PollerEvent(): m_Events{} {}
    ~PollerEvent() = default;


    //! epoll的事件是uint32_t，poll的事件是short
    // explicit PollerEvent(const int val): m_Events(val) {}
    // explicit PollerEvent(const uint32_t val): m_Events(val) {}
    // explicit PollerEvent(const short val): m_Events(val) {}
    // operator int()      const { return m_Events; }
    // operator uint32_t() const { return m_Events; }
    // operator short()    const { return m_Events; }

    enum EventType {
        eNoneEvent  = 0,
        eReadEvent  = 1 << 0,
        eWriteEvent = 1 << 1,
        eErrorEvent = 1 << 2,
        eCloseEvent = 1 << 3,
    };

    /*! 添加感兴趣/发生的事件 !*/
    void AddEvent(const EventType event) { m_Events |= event; };
    void AddReadEvent () { m_Events |= eReadEvent; };
    void AddWriteEvent() { m_Events |= eWriteEvent; };
    void AddErrorEvent() { m_Events |= eWriteEvent; };
    void AddCloseEvent() { m_Events |= eCloseEvent; };

    void DelEvent(const EventType event) { m_Events &= ~event; };
    void ClrEvent() { m_Events = eNoneEvent; };
    void SetEvent(const EventType event)  { m_Events = event; };
    int  GetEvent() const { return m_Events; };

    /*! HandleEvent中用来判断事件的类型 !*/
    bool HasNoneEvent() const { return m_Events == eNoneEvent; }
    bool HasEvent(const EventType event) const { return m_Events & event; };
    bool HasCloseEvent() const { return HasEvent(eCloseEvent); }
    bool HasErrorEvent() const { return HasEvent(eErrorEvent) ; }
    bool HasReadEvent()  const { return HasEvent(eReadEvent); }
    bool HasWriteEvent() const { return HasEvent(eWriteEvent); }

private:
    int m_Events;
};



}

#endif //LINUXGAMESERVER_POLLEREVENT_H

