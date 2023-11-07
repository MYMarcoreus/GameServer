#ifndef LINUXGAMESERVER_EVENTLOOPTHREAD_H
#define LINUXGAMESERVER_EVENTLOOPTHREAD_H

#include <thread>
#include <mutex>
#include <future>
#include <functional>
#include "net_definations.h"


namespace yy::net {

class EventLoop;

class EventLoopThread {
public:

    EventLoopThread(F_ThreadInitCallback init_cb);
    ~EventLoopThread();

    EventLoop * StartLoop();

    uint64_t GetThreadID() const;

private:
    void ThreadLoopFunction(std::promise<EventLoop *> & loopPromise);

    EventLoop *            m_Loop;
    F_ThreadInitCallback   m_ThreadInitCallback;
    std::thread            m_LoopThread;
    std::once_flag         m_OnceFlag;

};

}

#endif //LINUXGAMESERVER_EVENTLOOPTHREAD_H
