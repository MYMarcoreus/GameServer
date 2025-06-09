#ifndef IOCPPOLLER_H
#define IOCPPOLLER_H

#ifdef ____WINDOWS

#include "Poller.h"

#include <winsock2.h>
#include <mswsock.h>
#include <windows.h>

namespace yy::net
{

class IOCPPoller final : public Poller {
public:
    explicit IOCPPoller(EventLoop *loop);
    ~IOCPPoller() override;

    ///@brief 执行epoll_wait，并将发生的事件channel填入`activeChannel`
    virtual void PollWait(ChannelList &activeChannel, std::chrono::milliseconds timeout) override;

    ///@brief 其实是一个状态机，让Channel的状态转移到下一个状态：对channel映射表和epoll监视列表进行增删覆盖操作
    virtual void UpdateChannel(IOChannel *) override;

    ///@brief 其实是一个状态机，让Channel的状态转移到下一个状态：彻底删除channel
    virtual void RemoveChannel(IOChannel *) override;

private:
    HANDLE m_iocpHandle;
};

}

#endif
#endif
