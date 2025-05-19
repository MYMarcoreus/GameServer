#include "ThreadPool.h"
#include "log.h"
#include "EventLoop.h"
#include <functional>
#include <iostream>

namespace yy::net {

using namespace yy::util;

ThreadPool::ThreadPool(std::string name, int queueSize)
        : m_name(name)
        , m_IsRunning(false)
        , m_Threads{}
        , m_Queue(queueSize)
        // , m_TimerManager{nullptr}
{}

ThreadPool::~ThreadPool() {
    while(m_IsRunning)
    {
        Stop();
    }
}

void ThreadPool::Start(net::EventLoop * timerloop, int threadNum) {
    m_IsRunning = true;
    m_Threads.resize(threadNum);

    m_TimerLoop = timerloop;

    // m_TimerManager = std::make_unique<yy::net::TimerManager>(timerloop);

    for(std::thread & work_thread: m_Threads)
    {
        work_thread = std::thread{&ThreadPool::PopAndExecuteTask, this};
        YLOG_INFO("启动线程池[{}]线程<{}>", m_name, CastThreadIDToStr(work_thread.get_id()))
    }
}

void ThreadPool::Stop() {
    if(m_IsRunning) {
        m_IsRunning = false;

        // 唤醒所有线程，然后所有线程检查pop出来的是否为不合法的元素，如果是，检查m_IsRunning
        for (int i = 0; i < m_Threads.size(); ++i) {
            m_Queue.try_push(nullptr);
        }

        // 等待所有线程执行完
        for(std::thread & work_thread: m_Threads)
        {
            if(work_thread.joinable())
                work_thread.join();
        }
    }

}

void ThreadPool::PushTask(Task task) {
    m_Queue.wait_push(task); // 为无界队列时就不用wait
}

void ThreadPool::PopAndExecuteTask() {
    while (m_IsRunning)
    {
        // 等待任务到来，取出任务
        Task task{};
        m_Queue.wait_pop(task);

        // wait结束可能是来自Stop()函数，因此要判断是否为结束信号。
        if(!m_IsRunning)
            break;

        // 执行任务
        if(task)
            task();
    }

    std::cout << "thread<" << std::this_thread::get_id() << "> finished!\n";
}

net::TimerID ThreadPool::RunTaskAt(net::Timestamp time, Task cb) {
    return m_TimerLoop->RunAt(time, [this, taskcb = std::move(cb)](){this->PushTask(taskcb);});
}

net::TimerID ThreadPool::RunTaskAfter(net::Microseconds delay, Task cb) {
    return m_TimerLoop->RunAfter(delay, [this, taskcb = std::move(cb)](){this->PushTask(taskcb);});
}

net::TimerID ThreadPool::RunTaskEvery(net::Microseconds interval, Task cb) {
    return m_TimerLoop->RunEvery(interval, [this, taskcb = std::move(cb)](){ this->PushTask(taskcb); });
}

void ThreadPool::CancelTimer(net::TimerID timerid) {
    m_TimerLoop->CancelTimer(timerid);
}


} // yy::util
