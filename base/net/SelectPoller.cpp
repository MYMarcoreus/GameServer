#include "SelectPoller.h"
#include "Channel.h"
#include "Timestamp.h"
#include <cassert>


namespace yy::net {


using SocketApiWrapper::socket_t;


namespace ____detail{

class FdSet
{
private:
    fd_set fdset_{};
public:
    FdSet() { this->clear(); }

    // 将fdset变量所有位归0
    void clear() { FD_ZERO(&fdset_); }

    // 将第fd号位置1：将文件描述符fd加入监视列表
    void add_fd(int fd) { FD_SET(fd, &fdset_); }

    // 将第fd号位归0：将文件描述符fd的从监视列表中清除
    void del_fd(int fd) { FD_CLR(fd, &fdset_); }

    // 查询第fd号位：查询文件描述符fd是否在监视列表中
    bool has_fd(int fd) { return FD_ISSET(fd, &fdset_); }

    fd_set & get_fdset() { return fdset_; }
};

}






SelectPoller::SelectPoller(EventLoop* loop) : Poller(loop)
{
    FD_ZERO(&select_readfds_);
    FD_ZERO(&select_writefds_);
    FD_ZERO(&select_expectfds_);
    FD_ZERO(&write_fds_);
    FD_ZERO(&read_fds_);
    FD_ZERO(&expect_fds_);
}

void SelectPoller::UpdateChannel(Channel* channel)
{
    if (polledChannelsMap_.find(channel->GetFD()) == polledChannelsMap_.end())
    {
        polledChannelsMap_.insert(std::pair<socket_t, Channel*>(channel->GetFD(), channel));
        fdSet_.insert(channel->GetFD());
    }
    Update(channel);
}

void SelectPoller::RemoveChannel(Channel* channel)
{
    assert(channel);
    socket_t fd = channel->GetFD();
    assert(polledChannelsMap_.find(fd) != polledChannelsMap_.end());
    assert(fdSet_.find(fd) != fdSet_.end());
    FD_CLR(fd, &select_readfds_);
    FD_CLR(fd, &select_writefds_);
    FD_CLR(fd, &select_expectfds_);
    polledChannelsMap_.erase(fd);
    fdSet_.erase(fd);
}

void SelectPoller::Update(Channel* channel)
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

    read_fds_ = select_readfds_;
    write_fds_ = select_writefds_;
    expect_fds_ = select_expectfds_;
    int maxFd = 0;
    if (!fdSet_.empty())
    {
        maxFd = *(fdSet_.begin());
    }

    int numActiveEvents = ::select(maxFd + 1, &read_fds_, &write_fds_, &expect_fds_, &tv);

    if (numActiveEvents > 0) {
        FillActiveChannel(activeChannels, numActiveEvents);
    }
    else if (!numActiveEvents) {
        //
    }
    else
    {
#ifdef ON_WINDOWS
        int err = WSAGetLastError();
			LOG_ERROR << "select system call error, info:"
                << " errno:" << err
                << strerror(err)
                << " read fdcount:" << read_fds_.fd_count
                << " write fdcount:" << write_fds_.fd_count;
#endif
    }
}

void SelectPoller::FillActiveChannel(ChannelList & activeChannels, int numEvents)
{
    int readyEvent = 0;
    for (auto it = fdSet_.begin(); it != fdSet_.end() && numEvents > 0; it++)
    {
        socket_t fd = *it;
        if (FD_ISSET(fd, &read_fds_  )) readyEvent |= PollerEvent::eReadEvent;
        if (FD_ISSET(fd, &write_fds_ )) readyEvent |= PollerEvent::eWriteEvent;
        if (FD_ISSET(fd, &expect_fds_)) readyEvent |= PollerEvent::eErrorEvent;

        if (readyEvent != PollerEvent::eNoneEvent)
        {
            numEvents--;
            Channel* channel = polledChannelsMap_.find(fd)->second;
            assert(channel);
            channel->SetHappendedEvent(readyEvent);
            activeChannels.push_back(channel);
        }
    }
}

















}