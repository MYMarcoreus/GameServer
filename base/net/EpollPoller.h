#ifdef ____LINUX




#ifndef LINUXGAMESERVER_EPOLLPOLLER_H
#define LINUXGAMESERVER_EPOLLPOLLER_H


#include "Poller.h"
#include <cstdint>
#include <sys/epoll.h>
#include <vector>

namespace yy::net {

class EpollPoller: public Poller {
public:
    EpollPoller(EventLoop *loop);
    ~EpollPoller();

    ///@brief 执行epoll_wait，并将发生的事件channel填入`activeChannel`
    virtual void PollWait(ChannelList &activeChannel, std::chrono::milliseconds timeout) override;

    ///@brief 其实是一个状态机，让Channel的状态转移到下一个状态：对channel映射表和epoll监视列表进行增删覆盖操作
    virtual void UpdateChannel(Channel *) override;

    ///@brief 其实是一个状态机，让Channel的状态转移到下一个状态：彻底删除channel
    virtual void RemoveChannel(Channel *) override;

private:
    ///@brief PollWait返回后调用
    void FillActiveChannels(ChannelList & activeChannel, int numEvents);

    ///@brief epoll_ctl的接口函数，修改epoll监视列表
    void SetEpollOperation(Channel * channel, int EPOLL_CTL_XXX);

private:
    int                             m_EpollFD;
    std::vector<struct epoll_event> m_EpollEventList; // 在epoll_wait中使用，存放发生的事件
    bool                            m_useET;
    static const int                kEpollMaxSize = 16;
};

}

#endif //LINUXGAMESERVER_EPOLLPOLLER_H





#endif

