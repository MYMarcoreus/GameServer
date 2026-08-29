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
        YLOG_INFO("[EventLoopThreadPool] 启动io线程<{}>！", loop_thread->GetThreadID())
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
    //! 线程安全轮转分配：Start() 之后 m_ioLoops 只读，m_NextLoop 使用原子自增。
    if (m_ioLoops.empty()) {
        return m_BaseLoop;
    }
    const auto idx = m_NextLoop.fetch_add(1, std::memory_order_relaxed);
    return m_ioLoops[idx % m_ioLoops.size()];
}




}
