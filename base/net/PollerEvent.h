#ifndef LINUXGAMESERVER_POLLEREVENT_H
#define LINUXGAMESERVER_POLLEREVENT_H

#ifdef ____Linux

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

#ifdef
#include <winsock2.h>
#endif

namespace yy::net {

// 集Epoll和Poll的事件的共同点
struct PollerEvent final
{
public:
    PollerEvent(): m_Events{} {}
    ~PollerEvent() = default;


    //! epoll的事件是uint32_t，poll的事件是short
    PollerEvent(int val): m_Events(val) {}
    PollerEvent(uint32_t val): m_Events(val) {}
    PollerEvent(short val): m_Events(val) {}
    operator int()      const { return m_Events; }
    operator uint32_t() const { return m_Events; }
    operator short()    const { return m_Events; }

    enum EventType {
        eNoneEvent = 0,
        eIN    = POLLIN,
        ePRI   = POLLPRI,
        eOUT   = POLLOUT,
        eWriteEvent = eOUT,
        // eRDHUP = POLLRDHUP,
        eReadEvent = eIN | ePRI /*| eRDHUP*/,
        eERR   = POLLERR,
        eHUP   = POLLHUP,
        eRDNORM = POLLRDNORM,
        eRDBAND = POLLRDBAND,
        eWRNORM = POLLWRNORM,
        eWRBAND = POLLWRBAND,
        eNVAL   = POLLNVAL,
        eErrorEvent = eNVAL | eERR,
    };

    /*! 添加感兴趣的事件 !*/
    void AddEvent(const EventType event) { m_Events |= event; };
    void DelEvent(const EventType event) { m_Events &= ~event; };
    void ClrEvent() { m_Events = eNoneEvent; };
    void SetEvent(const EventType event)  { m_Events = event; };
    int  GetEvent() const { return m_Events; };
    bool HasEvent(const EventType event) const { return m_Events & event; };
    bool HasNoneEvent() { return m_Events == eNoneEvent; }

    /*! HandleEvent中用来判断事件的类型 !*/
    bool IsCloseEvent() const { return HasEvent(eHUP) && !HasEvent(eIN); }
    bool IsErrorEvent() const { return HasEvent(eErrorEvent) ; }
    bool IsReadEvent()  const { return HasEvent(eReadEvent); }
    bool IsWriteEvent() const { return HasEvent(eWriteEvent); }

private:
    int m_Events;
};



}

#endif //LINUXGAMESERVER_POLLEREVENT_H
