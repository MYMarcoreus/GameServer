#pragma once

#include <set>
#include "net_definations.h"
#include "socket_definations.h"
#include "Poller.h"


namespace yy::net {


class SelectPoller final : public Poller {
public:
    explicit SelectPoller(EventLoop *loop);
    ~SelectPoller() override = default;

    ///@brief 执行epoll_wait，并将发生的事件channel填入`activeChannel`
    virtual void PollWait(ChannelList &activeChannel, Milliseconds timeout/* = std::chrono::milliseconds::max()*/) override;

    ///@brief 其实是一个状态机，让Channel的状态转移到下一个状态：对channel映射表和epoll监视列表进行增删覆盖操作
    virtual void UpdateChannel(IOChannel *) override;

    ///@brief 其实是一个状态机，让Channel的状态转移到下一个状态：彻底删除channel
    virtual void RemoveChannel(IOChannel *) override;
private:
    void FillActiveChannel(ChannelList & activeChannel, int numEvents);

    void Update(IOChannel* channel);

private:
    fd_set select_readfds_;
    fd_set select_writefds_;
    fd_set select_expectfds_;
    fd_set happended_writefds_;
    fd_set happended_readfds_;
    fd_set happended_expectfds_;

    // 用于获取最大的套接字描述符，以规定select的遍历范围
    std::set<SocketApiWrapper::socket_t, std::greater<SocketApiWrapper::socket_t> > fdSet_;
};


}

