#include "RBTreeTimerManager.h"
#include "PriorityQueueTimerManager.h"

using namespace yy::net;

TimerManager* TimerManager::NewDefaultTimerManager(EventLoop * loop)
{
#ifdef ____WINDOWS
    return new PriorityQueueTimerManager{loop};
#endif

#ifdef ____LINUX
    return new RBTreeTimerManager{loop};
#endif
}

