#ifdef ____LINUX



#include "EpollPoller.h"
#include "IOChannel.h"
#include "log.h"
#include "ErrnoSaver.h"
#include "status/Status.h"
#include <sys/epoll.h>

#include <cstdio>
#include <unistd.h>


namespace yy::net {




// 管理某个套接字的事件
class EpollPollerEvent
{
    friend class EpollPoller;
public:
    EpollPollerEvent(struct epoll_event ev = epoll_event{}): epoll_event_(ev)  { }
    explicit operator epoll_event() const { return epoll_event_; }

    ///@brief 返回epoll原生的数据结构epoll_event
    struct epoll_event * GetRawEvent() { return &epoll_event_; };

    //Region GETTER
    /* * data.ptr用于指定和data.fd相关的数据结构，但因为data是union，data只能存储ptr或fd
       * 所以data.ptr指向的数据结构需要包含fd：在这里我们的ptr指向channel */
    void *   GetHanppededPtr()   const { return epoll_event_.data.ptr; }
    int      GetFD()             const { return epoll_event_.data.fd; }
    uint32_t GetHanppendEvents() const { return epoll_event_.events;   }
    //End GETTER

    //Region SETTER
    // void SetInterestedEvents(const uint32_t ev) { epoll_event_.events = ev; }
    void SetInterestedPtr(void * p) { epoll_event_.data.ptr = p; }
    void SetInterestedFD(const int fd) { epoll_event_.data.fd = fd; }
    //End SETTER

    // 增加一个监视事件
    // void AddEvent(const EPOLL_EVENTS EPOLLXXXX) { epoll_event_.events |=  EPOLLXXXX; }
    void AddWriteEvent() { epoll_event_.events |=  EPOLLOUT; }
    void AddReadEvent () { epoll_event_.events |=  (EPOLLIN | EPOLLRDHUP); }
    void SetET() { epoll_event_.events |=  EPOLLET; }

    // 删除一个监视事件
    void DelEvent(const EPOLL_EVENTS EPOLLXXXX) { epoll_event_.events &= ~EPOLLXXXX; }

    // 在`epoll()`返回后，使用该函数查看事件是否发生
    // bool IsOccured(const EPOLL_EVENTS EPOLLXXXX) const { return (epoll_event_.events & EPOLLXXXX); }
    bool IsOccuredWrite() const { return (epoll_event_.events  &  EPOLLOUT); }
    bool IsOccuredRead () const { return (epoll_event_.events  & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)); }
    bool IsOccuredClose() const { return !(epoll_event_.events &  EPOLLIN) && (epoll_event_.events & (EPOLLHUP)); }
    bool IsOccuredError() const { return (epoll_event_.events  & (EPOLLERR)); }

private:
    struct epoll_event epoll_event_;
};





EpollPoller::EpollPoller(EventLoop *loop)
        : Poller(loop),
          m_EpollFD{::epoll_create1(EPOLL_CLOEXEC)},
          m_EpollEventList{kEpollMaxSize}
{
    if (m_EpollFD < 0) {
        fprintf(stderr,"epoll_create1() error");
    }
}

EpollPoller::~EpollPoller() {
    ::close(m_EpollFD);
}

void EpollPoller::PollWait(ChannelList &activeChannel, const std::chrono::milliseconds timeout) {
    int numEvents = epoll_wait(m_EpollFD, &*m_EpollEventList.begin(),
                               static_cast<int>(m_EpollEventList.size()),
                               timeout == std::chrono::milliseconds::max() ? -1 : timeout.count());
    ::yy::util::ErrnoSaver savedErrno{};
    if(numEvents > 0) {
        YLOG_TRACE("epoll_wait() return {} events, m_EpollEventList.size = {}", numEvents, m_EpollEventList.size())

        // 将发生了事件的channel加入activeChannel
        FillActiveChannels(activeChannel, numEvents);

        // 动态调整m_EpollEventList大小
        if (static_cast<size_t>(numEvents) == m_EpollEventList.size()) {
            m_EpollEventList.resize(static_cast<size_t>(numEvents * 2)); // NOLINT(bugprone-misplaced-widening-cast)
        }
    }
    else if(numEvents == 0) {
        YLOG_TRACE("epoll_wait() nothing happended.")
    } else {
        //! 不处理EINTR，对于其它错误，并不会让程序终止
        if(savedErrno != EINTR)  {
            errno = savedErrno;
            YLOG_ERROR("epoll_wait() error: {}", yy::util::GetErrorInfo(savedErrno))
        }
    }
}

void EpollPoller::UpdateChannel(IOChannel * channel) {
    AssertInLoopingThread();

    switch (channel->GetState()) {
        case IOChannel::State::eNew:
            //! 加入channel映射表（注意eDeleted状态的channel仍在映射表中，只是不在epoll监视列表中）
            m_ChannelMap[channel->GetFD()] = channel;
        case IOChannel::State::eDeleted: {
            //! 加入epoll监视列表
            UpdateEpollOperation(channel, EPOLL_CTL_ADD);
            channel->SetState(IOChannel::State::eAdded); //* 状态转换: eNew/eDeleted -> eAdded
            break;
        }
        case IOChannel::State::eAdded: {
            if(channel->IsNoneEvent()) {
                //! 只是从epoll底层数据结构中删除，并不从channel映射表中删除
                UpdateEpollOperation(channel, EPOLL_CTL_DEL);
                channel->SetState(IOChannel::State::eDeleted); //* 状态转换: eAdded -> eDeleted
                YLOG_TRACE("已将Channel<{}>从epoll底层删除，但是仍在channel映射表中", channel->GetFD())
            } else {
                //! 覆盖原来的事件
                UpdateEpollOperation(channel, EPOLL_CTL_MOD);
            }

            break;
        }
    }
}

void EpollPoller::RemoveChannel(IOChannel * channel) {
    AssertInLoopingThread();

    //! 完全删除channel（不仅从epoll底层数据结构中删除，也从channel映射表中删除）
    if(channel->GetState() == IOChannel::State::eAdded) {
        UpdateEpollOperation(channel, EPOLL_CTL_DEL);
    }
    channel->SetState(IOChannel::State::eNew); //* 状态转换: eAdded -> eNew
    m_ChannelMap.erase(channel->GetFD());
    YLOG_TRACE("已完全删除Channel<{}>", channel->GetFD())
}

void EpollPoller::FillActiveChannels(ChannelList & activeChannel, int numEvents) {
    for (int i = 0; i < numEvents; ++i) {
        EpollPollerEvent happend_events{m_EpollEventList[i]};

        /*! 找到发生了事件的event，找到跟它对应的channel，设置channel发生的事件并将该channel加入activeChannel
         慢一点的方法：使用m_ChannelMap::find()按照happend_event.GetFD()查找m_EpollList， */
        IOChannel * channel = static_cast<IOChannel*>(happend_events.GetHanppededPtr()); //

        // 设置发生了的事件
        PollerEvent events{};
        if (happend_events.IsOccuredRead ()) events.AddReadEvent();
        if (happend_events.IsOccuredWrite()) events.AddWriteEvent();
        if (happend_events.IsOccuredError()) events.AddErrorEvent();
        if (happend_events.IsOccuredClose()) events.AddCloseEvent();

        channel->SetHappendedEvent(events);
        activeChannel.push_back(channel);
    }
}

void EpollPoller::UpdateEpollOperation(IOChannel * channel, const int EPOLL_CTL_XXX) {
    EpollPollerEvent interested_events;

    //! 设置要监听的事件
    PollerEvent events = channel->GetInterestedEvent();
    if (events.HasReadEvent ()) { interested_events.AddReadEvent(); interested_events.SetET(); }
    if (events.HasWriteEvent()) { interested_events.AddWriteEvent(); }

    interested_events.SetInterestedPtr(channel); // 将和fd相关联的channel保存至data.ptr中

    ::epoll_ctl(m_EpollFD, EPOLL_CTL_XXX, channel->GetFD(), interested_events.GetRawEvent());
}


}



#endif

