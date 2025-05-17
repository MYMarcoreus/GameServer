#include "Poller.h"
#include "IOChannel.h"
#include "EventLoop.h"

namespace yy::net {


Poller::Poller(EventLoop *loop) : m_OwnerLoop(loop) {}

bool Poller::HasChannel(IOChannel *channel) {
    auto it = m_ChannelMap.find(channel->GetFD());
    return it != m_ChannelMap.end() && it->second == channel;
}


void Poller::AssertInLoopingThread() const {
    m_OwnerLoop->AssertInLoopingThread(__FILE__, __LINE__);
}


Poller::~Poller() = default;

} // yy::util
