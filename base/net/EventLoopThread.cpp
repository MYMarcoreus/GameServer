#include "EventLoopThread.h"
#include "EventLoop.h"
#include "util_functions.h"

namespace yy::net {

using namespace yy::util;

EventLoopThread::EventLoopThread(F_ThreadInitCallback init_cb)
    : m_Loop(nullptr),
    m_ThreadInitCallback(init_cb)
{ }

EventLoopThread::~EventLoopThread() {
    m_LoopThread.join();
}

EventLoop *EventLoopThread::StartLoop() {
    //! 防止多次启动
    std::call_once(m_OnceFlag,
        [this]()
        {
            std::promise<EventLoop *> loopPromise;

            //! 启动线程
            m_LoopThread = std::thread(
                [this, &loopPromise]() {
                    ThreadLoopFunction(loopPromise);
                }
            );
            //! 等待Loop线程初始化m_Loop
            loopPromise.get_future().wait(); // wait
        }
    );

    return m_Loop;
}

void EventLoopThread::ThreadLoopFunction(std::promise<EventLoop *> & loopPromise) {
    //! 使用栈上的EventLoop线程对象
    EventLoop eventLoop(true);

    //! 执行线程初始化回调函数
    if(m_ThreadInitCallback) {
        m_ThreadInitCallback(&eventLoop);
    }
    //! 初始化m_Loop并通知其它线程
    m_Loop = &eventLoop;
    loopPromise.set_value(&eventLoop); // notify

    //! 执行Loop
    m_Loop->Loop();

    //! Loop退出，指针置空，函数退出时栈上的对象自动释放
    m_Loop = nullptr;
}

uint64_t EventLoopThread::GetThreadID() const {
    return CastThreadIDToInt(m_LoopThread.get_id());
}


}