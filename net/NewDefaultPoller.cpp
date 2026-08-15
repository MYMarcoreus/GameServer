#include "EpollPoller.h"
#include "SelectPoller.h"

namespace yy::net {

Poller * Poller::NewDefaultPoller(EventLoop *loop)
{
#ifdef ____LINUX
    return new EpollPoller(loop);
#endif

#ifdef ____WINDOWS
    return new SelectPoller(loop);
#endif
}


}
