#ifdef ____LINUX



#include "EpollPoller.h"
#include "Channel.h"
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
    operator epoll_event() { return epoll_event_; }

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
    void SetInterestedPtr(void * p) { epoll_event_.data.ptr = p; }
    void SetInterestedEvents(uint32_t ev) { epoll_event_.events = ev; }
    void SetInterestedFD(int fd) { epoll_event_.data.fd = fd; }
    //End SETTER

    // 增加一个监视事件
    void AddEvent(EPOLL_EVENTS EPOLLXXXX) { epoll_event_.events |=  EPOLLXXXX; }

    // 删除一个监视事件
    void DelEvent(EPOLL_EVENTS EPOLLXXXX) { epoll_event_.events &= ~EPOLLXXXX; }

    // 在`epoll()`返回后，使用该函数查看事件是否发生
    bool IsOccured(EPOLL_EVENTS EPOLLXXXX) const { return (epoll_event_.events & EPOLLXXXX); }

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

void EpollPoller::PollWait(ChannelList &activeChannel, std::chrono::milliseconds timeout) {
    int numEvents = epoll_wait(m_EpollFD, &*m_EpollEventList.begin(),
                               (int) m_EpollEventList.size(), timeout == std::chrono::milliseconds::max() ? -1 : timeout.count());
    ::yy::util::ErrnoSaver savedErrno;
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

void EpollPoller::UpdateChannel(Channel * channel) {
    AssertInLoopingThread();

    switch (channel->GetState()) {
        case Channel::State::eNew:
        case Channel::State::eDeleted: {
            //! 加入channel映射表（注意kDeleted状态的channel仍在映射表中，只是不在epoll监视列表中）
            if(channel->GetState() == Channel::State::eNew) {
                m_ChannelMap[channel->GetFD()] = channel;
            }

            //! 加入epoll监视列表
            SetEpollOperation(channel, EPOLL_CTL_ADD);
            channel->SetState(Channel::State::eAdded); //* 状态转换: eNew/eDeleted -> eAdded
            break;
        }
        case Channel::State::eAdded: {
            if(channel->IsNoneEvent()) {
                //! 只是从epoll底层数据结构中删除，并不从channel映射表中删除
                SetEpollOperation(channel, EPOLL_CTL_DEL);
                channel->SetState(Channel::State::eDeleted); //* 状态转换: eAdded -> eDeleted
                YLOG_TRACE("已将Channel<{}>从epoll底层删除，但是仍在channel映射表中", channel->GetFD())
            } else {
                //! 覆盖原来的事件
                SetEpollOperation(channel, EPOLL_CTL_MOD);
            }

            break;
        }
    }
}

void EpollPoller::RemoveChannel(Channel * channel) {
    AssertInLoopingThread();

    //! 完全删除channel（不仅从epoll底层数据结构中删除，也从channel映射表中删除）
    if(channel->GetState() == Channel::State::eAdded) {
        SetEpollOperation(channel, EPOLL_CTL_DEL);
    }
    channel->SetState(Channel::State::eNew); //* 状态转换: eAdded -> eNew
    m_ChannelMap.erase(channel->GetFD());
    YLOG_TRACE("已完全删除Channel<{}>", channel->GetFD())
}

void EpollPoller::FillActiveChannels(ChannelList & activeChannel, int numEvents) {
    for (int i = 0; i < numEvents; ++i) {
        EpollPollerEvent happend_event{m_EpollEventList[i]};

        /*! 找到发生了事件的event，找到跟它对应的channel，设置channel发生的事件并将该channel加入activeChannel
         慢一点的方法：使用m_ChannelMap::find()按照happend_event.GetFD()查找m_EpollList， */
        Channel * channel = static_cast<Channel*>(happend_event.GetHanppededPtr()); //
        channel->SetHappendedEvent(happend_event.GetHanppendEvents());
        activeChannel.push_back(channel);
    }
}

void EpollPoller::SetEpollOperation(Channel * channel, int EPOLL_CTL_XXX) {
    EpollPollerEvent new_event;
    new_event.SetInterestedEvents(channel->GetInterestedEvent());
    new_event.SetInterestedPtr(channel); // 将和fd相关联的channel保存至data.ptr中
    new_event.AddEvent(EPOLLET); //! ET

    ::epoll_ctl(m_EpollFD, EPOLL_CTL_XXX, channel->GetFD(), new_event.GetRawEvent());
}


}



#endif

