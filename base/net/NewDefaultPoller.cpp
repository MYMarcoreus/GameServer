#include "EpollPoller.h"

namespace yy::net {

Poller * Poller::NewDefaultPoller(EventLoop *loop, bool useETIfEpoller)
{
#ifdef ____LINUX
    return new EpollPoller(loop, useETIfEpoller);
#endif

#ifdef ____WINDOWS
    return new SelectPoller(loop, useETIfEpoller);
#endif
}


}