#ifndef LINUXGAMESERVER_EVENTLOOPTHREADPOOL_H
#define LINUXGAMESERVER_EVENTLOOPTHREADPOOL_H

#include <vector>
#include <functional>
#include <memory>
#include "net_definations.h"

namespace yy::net {

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool {
public:
    EventLoopThreadPool(EventLoop *baseLoop);
    ~EventLoopThreadPool();

    void Start(int threadNum, F_CloseShutdownConnectionsCallback CloseShutdownCallbacks, F_ThreadInitCallback cb = F_ThreadInitCallback());


    ///@brief 轮转法获得下一个EventLoop
    ///todo ：可以用更复杂的调度法
    EventLoop * GetNextLoop();
private:
    /*
     * ThreadNum:
     *      0  ：所有的IO都在BaseLoop中执行
     *      1  ：所有的IO都在另一线程中执行
     *      N  ：以轮转法将新连接分配给线程池中的N个线程
     * */
    EventLoop *                                   m_BaseLoop;
    std::vector<EventLoop*>                       m_Loops;
    std::vector<std::unique_ptr<EventLoopThread>> m_Threads;

    int m_NextLoop;
};

}


#endif //LINUXGAMESERVER_EVENTLOOPTHREADPOOL_H
