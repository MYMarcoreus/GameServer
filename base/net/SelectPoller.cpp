#include "SelectPoller.h"
#include "IOChannel.h"
#include "Timestamp.h"
#include "log.h"
#include "util_functions.h"
#include "PollerEvent.h"
#include <cassert>


namespace yy::net {


using SocketApiWrapper::socket_t;


// 可以使用，但是如果定义不暴露给外部的话，需要用指针当成员，这样更麻烦了。
class FdSet
{
private:
    fd_set fdset_{};
public:
    FdSet() { this->clear(); }

    // 将fdset变量所有位归0
    void clear() { FD_ZERO(&fdset_); }

    // 将第fd号位置1：将文件描述符fd加入监视列表
    void add_fd(const int fd) { FD_SET(fd, &fdset_); }

    // 将第fd号位归0：将文件描述符fd的从监视列表中清除
    void del_fd(const int fd) { FD_CLR(fd, &fdset_); }

    // 查询第fd号位：查询文件描述符fd是否在监视列表中
    bool has_fd(const int fd) const { return FD_ISSET(fd, &fdset_); }

    fd_set & get_fdset() { return fdset_; }
};








SelectPoller::SelectPoller(EventLoop* loop) : Poller(loop)
{
    FD_ZERO(&select_readfds_);
    FD_ZERO(&select_writefds_);
    FD_ZERO(&select_expectfds_);
    FD_ZERO(&happended_writefds_);
    FD_ZERO(&happended_readfds_);
    FD_ZERO(&happended_expectfds_);
}

void SelectPoller::UpdateChannel(IOChannel* channel)
{
    // Channel的，Channel调用EventLoop调用本函数(Poller::UpdateChannel)以更改Channel对应的系统套接字
    socket_t fd = channel->GetFD();

    // 更改自定义数据结构
    if (!m_ChannelMap.contains(fd)) {
        m_ChannelMap.emplace(fd, channel);
        fdSet_.insert(fd);
    }

    // 更改系统底层数据结构
    Update(channel);
}


void SelectPoller::RemoveChannel(IOChannel* channel)
{
    assert(channel);
    const socket_t fd = channel->GetFD();
    assert(m_ChannelMap.contains(fd));
    assert(fdSet_.contains(fd));
    FD_CLR(fd, &select_readfds_);
    FD_CLR(fd, &select_writefds_);
    FD_CLR(fd, &select_expectfds_);
    m_ChannelMap.erase(fd);
    fdSet_.erase(fd);
}

void SelectPoller::Update(IOChannel* channel)
{
    assert(channel);

    FD_SET(channel->GetFD(), &select_expectfds_);

    if (channel->IsEnableReading()) {
        FD_SET(channel->GetFD(), &select_readfds_);
    }
    else {
        FD_CLR(channel->GetFD(), &select_readfds_);
    }

    if (channel->IsEnableWriting()) {
        FD_SET(channel->GetFD(), &select_writefds_);
    }
    else {
        FD_CLR(channel->GetFD(), &select_writefds_);
    }

    if (channel->IsNoneEvent())
    {
        FD_CLR(channel->GetFD(), &select_readfds_);
        FD_CLR(channel->GetFD(), &select_writefds_);
        FD_CLR(channel->GetFD(), &select_expectfds_);
    }
}

void SelectPoller::PollWait(Poller::ChannelList &activeChannels, Milliseconds timeout_ms)
{
    struct timeval tv;
    tv.tv_sec = timeout_ms.count()/1000;
    tv.tv_usec = (timeout_ms.count() % 1000) * 1000;

    happended_readfds_ = select_readfds_;
    happended_writefds_ = select_writefds_;
    happended_expectfds_ = select_expectfds_;

    int maxFd = fdSet_.empty() ? 0 : *(fdSet_.begin());

    int numActiveEvents = ::select(maxFd + 1,
                                   &happended_readfds_,
                                   &happended_writefds_,
                                   &happended_expectfds_, &tv);

    if (numActiveEvents > 0) {
        FillActiveChannel(activeChannels, numActiveEvents);
    }
    else if (numActiveEvents == 0) {
        // nothing to do
    }
    else {
        YLOG_ERROR("Select failed<{}>", util::GetLastErrorInfo())
    }
}

void SelectPoller::FillActiveChannel(ChannelList& activeChannels, int numEvents)
{
    if (numEvents <= 0)
        return;

    for (socket_t fd : fdSet_) {

        PollerEvent readyEvent{};
        if (FD_ISSET(fd, &happended_readfds_))   readyEvent.AddReadEvent();
        if (FD_ISSET(fd, &happended_writefds_))  readyEvent.AddWriteEvent();
        if (FD_ISSET(fd, &happended_expectfds_)) readyEvent.AddErrorEvent();

        if (!readyEvent.HasNoneEvent()) {
            if (auto it = m_ChannelMap.find(fd); it != m_ChannelMap.end()) {
                IOChannel* channel = it->second;
                assert(channel);
                channel->SetHappendedEvent(readyEvent);
                activeChannels.push_back(channel);
                --numEvents;
            }
        }
    }
}


















}
