#pragma once
#ifdef ____LINUX


#include "Poller.h"
#include <cstdint>
#include <sys/epoll.h>
#include <vector>

namespace yy::net {

class EpollPoller final : public Poller {
public:
    explicit EpollPoller(EventLoop *loop);
    ~EpollPoller() override;

    ///@brief 执行epoll_wait，并将发生的事件channel填入`activeChannel`
    virtual void PollWait(ChannelList &activeChannel, std::chrono::milliseconds timeout) override;

    ///@brief 其实是一个状态机，让Channel的状态转移到下一个状态：对channel映射表和epoll监视列表进行增删覆盖操作
    virtual void UpdateChannel(IOChannel *) override;

    ///@brief 其实是一个状态机，让Channel的状态转移到下一个状态：彻底删除channel
    virtual void RemoveChannel(IOChannel *) override;

private:
    ///@brief PollWait返回后调用
    void FillActiveChannels(ChannelList & activeChannel, int numEvents);

    ///@brief epoll_ctl的接口函数，修改epoll监视列表
    void UpdateEpollOperation(IOChannel * channel, int EPOLL_CTL_XXX);

private:
    int                             m_EpollFD;
    std::vector<struct epoll_event> m_EpollEventList; // 在epoll_wait中使用，存放发生的事件
    static constexpr int            kEpollMaxSize = 16;
};

}

#endif
