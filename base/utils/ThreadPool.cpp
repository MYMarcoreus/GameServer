#include "ThreadPool.h"
#include "log.h"
#include "EventLoop.h"
#include "util_functions.h"
#include <functional>
#include <iostream>
#include <cassert>

namespace yy::net {

using namespace yy::util;

ThreadPool::ThreadPool(const std::string& name, const int queueSize)
        : m_name(name)
        , m_Queue(queueSize)
        , m_Threads{}
        , m_IsRunning(false)
        , m_TimerLoop{nullptr}
{}

ThreadPool::~ThreadPool() {
    while(m_IsRunning)
    {
        Stop();
    }
}

void ThreadPool::Start(EventLoop * timerloop, const int threadNum) {
    m_IsRunning = true;
    m_Threads.resize(threadNum);

    m_TimerLoop = timerloop;

    for(std::thread & work_thread: m_Threads)
    {
        work_thread = std::thread{&ThreadPool::PopAndExecuteTask, this};
        YLOG_INFO("启动线程池[{}]线程<{}>", m_name, CastThreadIDToStr(work_thread.get_id()))
    }
}

void ThreadPool::Stop() {
    if(m_IsRunning) {
        m_IsRunning = false;

        // 取消定时任务
        for (const auto timerid: m_runningTimerIDs) {
            CancelTimer(timerid);
        }

        // 唤醒所有线程，然后所有线程检查pop出来的是否为不合法的元素，如果是，检查m_IsRunning
        for (int i = 0; i < m_Threads.size(); ++i) {
            m_Queue.try_push(nullptr);
        }

        // 等待所有线程执行完
        for(std::thread & work_thread: m_Threads)
        {
            //!FIXEDBUG 防止work_thread在自己执行的task中来析构ThreadPool并执行了Stop，执行work_thread.join()时可能存在自我join的问题，从而导致以下问题：
            //! terminate called after throwing an instance of 'std::system_error' : what():  Resource deadlock avoided
            //! 以上问题发生在自取消的定时任务中
            if (work_thread.joinable() && work_thread.get_id() != std::this_thread::get_id()) {
                work_thread.join();
            }
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

    YLOG_INFO("结束线程池[{}]线程<{}>", m_name, CastThreadIDToStr(std::this_thread::get_id()))
}

std::optional<TimerID> ThreadPool::RunTaskAt(const Timestamp time, Task cb) {
    assert(m_IsRunning);
    if (m_TimerLoop == nullptr) {
        return std::nullopt;
    }
    auto timerid = m_TimerLoop->RunAt(time, [this, taskcb = std::move(cb)]{this->PushTask(taskcb);});
    m_runningTimerIDs.emplace_back(timerid);
    return timerid;
}

std::optional<TimerID> ThreadPool::RunTaskAfter(const Microseconds delay, Task cb) {
    assert(m_IsRunning);
    if (m_TimerLoop == nullptr) {
        return std::nullopt;
    }
    auto timerid = m_TimerLoop->RunAfter(delay, [this, taskcb = std::move(cb)](){this->PushTask(taskcb);});
    m_runningTimerIDs.emplace_back(timerid);
    return timerid;
}

std::optional<TimerID> ThreadPool::RunTaskEvery(const Microseconds interval, Task cb) {
    assert(m_IsRunning);
    if (m_TimerLoop == nullptr) {
        return std::nullopt;
    }
    auto timerid = m_TimerLoop->RunEvery(interval, [this, taskcb = std::move(cb)](){ this->PushTask(taskcb); });
    m_runningTimerIDs.emplace_back(timerid);
    return timerid;
}

void ThreadPool::CancelTimer(const TimerID timerid) {
    assert(m_TimerLoop);
    m_TimerLoop->CancelTimer(timerid);
}


} // yy::util
