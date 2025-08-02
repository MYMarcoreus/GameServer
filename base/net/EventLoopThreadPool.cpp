#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"
#include "EventLoop.h"
#include "log.h"

namespace yy::net {

EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop)
    : m_BaseLoop(baseLoop),
      m_NextLoop{0}
{ }

EventLoopThreadPool::~EventLoopThreadPool() {
    //! 不用释放EventLoop，因为EventLoop是栈上的对象，在EventLoop结束后会自动销毁，因此根本不用管理EventLoop的生命周期
}

void EventLoopThreadPool::Start(const int threadNum, Milliseconds pollwaitTimeout, F_ThreadInitCallback cb) {
    m_BaseLoop->AssertInLoopingThread();

    for (int i = 0; i < threadNum; ++i) {
        auto loop_thread = std::make_unique<EventLoopThread>(cb, pollwaitTimeout);
        m_ioLoops.emplace_back(loop_thread->CreateLoop());
        YLOG_INFO("启动io线程<{}>！", loop_thread->GetThreadID())
        m_Threads.emplace_back(std::move(loop_thread));
    }

    // 没有额外的线程，只有主线程，仍要执行线程初始化回调
    if(threadNum == 0 && cb) {
        cb(m_BaseLoop);
    }
}

void EventLoopThreadPool::StartTick(const int threadNum, Milliseconds pollwaitTimeout, const Milliseconds deltaTime, F_ThreadInitCallback cb) {
    m_BaseLoop->AssertInLoopingThread();

    for (int i = 0; i < threadNum; ++i) {
        auto loop_thread = std::make_unique<EventLoopThread>(cb, pollwaitTimeout);
        m_ioLoops.emplace_back(loop_thread->CreateLoopTick(deltaTime));
        YLOG_INFO("启动io线程<{}>！", loop_thread->GetThreadID())
        m_Threads.emplace_back(std::move(loop_thread));
    }

    // 没有额外的线程，只有主线程，仍要执行线程初始化回调
    if(threadNum == 0 && cb) {
        cb(m_BaseLoop);
    }
}

EventLoop *EventLoopThreadPool::GetNextLoop() {
    m_BaseLoop->AssertInLoopingThread();

    EventLoop * loop = nullptr;
    if(!m_ioLoops.empty()) {
        loop = m_ioLoops[m_NextLoop++];
        if(m_NextLoop >= m_ioLoops.size()) {
            m_NextLoop = 0;
        }
    } else {
        loop = m_BaseLoop;
    }

    return loop;
}




}
