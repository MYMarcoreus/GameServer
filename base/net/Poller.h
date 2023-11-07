#ifndef LINUXGAMESERVER_POLLER_H
#define LINUXGAMESERVER_POLLER_H

#include <map>
#include <vector>


namespace yy::net {

class EventLoop;
class Channel;


class Poller {
public:
    using ChannelList = std::vector<Channel *>;

    Poller(EventLoop * loop);
    virtual ~Poller();

    ///@brief 执行epoll或poll，将发生的事件填充至activeChannel，可设置超时时间timeout
    virtual void PollWait(ChannelList &activeChannel, int timeout = -1) = 0;

    ///@brief 在m_ChannelMap中更新channel
    virtual void UpdateChannel(Channel * channel) = 0;

    ///@brief 在m_ChannelMap中删除channel
    virtual void RemoveChannel(Channel * channel) = 0;

    ///@brief 在m_ChannelMap中查询
    bool HasChannel(Channel *channel);

    ///@brief 返回epoll或poll实现的Poller实现子类
    static Poller *NewDefaultPoller(EventLoop *loop, bool useETIfEpoller = false);

    //
    void AssertInLoopingThread() const;


protected:
    EventLoop *                 m_OwnerLoop;
    std::map<int, Channel *>    m_ChannelMap; //  get_fd->Channel*
};

} // yy::net

#endif //LINUXGAMESERVER_POLLER_H
