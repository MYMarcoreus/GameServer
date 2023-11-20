#ifndef GAMESERVER_SELECTPOLLER_H
#define GAMESERVER_SELECTPOLLER_H


#include <set>
#include <map>
#include "net_definations.h"
#include "socket_definations.h"
#include "Poller.h"
#include "PollerEvent.h"


namespace yy::net {


class SelectPoller: public Poller {
public:
    SelectPoller(EventLoop *loop);
    ~SelectPoller() = default;

    ///@brief 执行epoll_wait，并将发生的事件channel填入`activeChannel`
    virtual void PollWait(ChannelList &activeChannel, Milliseconds timeout = Milliseconds::max()) override;

    ///@brief 其实是一个状态机，让Channel的状态转移到下一个状态：对channel映射表和epoll监视列表进行增删覆盖操作
    virtual void UpdateChannel(Channel *) override;

    ///@brief 其实是一个状态机，让Channel的状态转移到下一个状态：彻底删除channel
    virtual void RemoveChannel(Channel *) override;
private:
    void FillActiveChannel(ChannelList & activeChannel, int numEvents);

    void Update(Channel* channel);

private:
    std::map<SocketApiWrapper::socket_t, Channel*> polledChannelsMap_;
    fd_set select_readfds_;
    fd_set select_writefds_;
    fd_set select_expectfds_;

    fd_set write_fds_;
    fd_set read_fds_;
    fd_set expect_fds_;

    std::set<SocketApiWrapper::socket_t, std::greater<SocketApiWrapper::socket_t> > fdSet_;
};


}


#endif //GAMESERVER_SELECTPOLLER_H

