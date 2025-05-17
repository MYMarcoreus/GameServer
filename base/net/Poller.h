#ifndef LINUXGAMESERVER_POLLER_H
#define LINUXGAMESERVER_POLLER_H

#include <unordered_map >
#include <vector>
#include <chrono>
#include "socket_definations.h"


namespace yy::net {

class EventLoop;
class IOChannel;


class Poller {
public:
    using ChannelList = std::vector<IOChannel *>;

    Poller(EventLoop * loop);
    virtual ~Poller();

    ///@brief 执行epoll或poll，将发生的事件填充至activeChannel，可设置超时时间timeout
    /// 阻塞：Milliseconds::max() / std::chrono::milliseconds::max()
    /// 非阻塞：0s
    virtual void PollWait(ChannelList &activeChannel, std::chrono::milliseconds timeout/* = std::chrono::milliseconds::max()*/) = 0;

    ///@brief 在m_ChannelMap中更新channel
    virtual void UpdateChannel(IOChannel * channel) = 0;

    ///@brief 在m_ChannelMap中删除channel
    virtual void RemoveChannel(IOChannel * channel) = 0;

    ///@brief 在m_ChannelMap中查询
    bool HasChannel(IOChannel *channel);

    ///@brief 返回epoll或poll实现的Poller实现子类
    static Poller *NewDefaultPoller(EventLoop *loop);

    //
    void AssertInLoopingThread() const;


protected:
    EventLoop *                 m_OwnerLoop;
    std::unordered_map <SocketApiWrapper::socket_t, IOChannel *>    m_ChannelMap; //  get_fd->Channel*
};

} // yy::net

#endif //LINUXGAMESERVER_POLLER_H

