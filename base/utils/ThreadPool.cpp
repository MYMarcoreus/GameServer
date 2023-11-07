#include "ThreadPool.h"
#include "log.h"
#include <functional>
#include <iostream>

namespace yy::util {


ThreadPool::ThreadPool(int queueSize) : m_IsRunning(false), m_Threads{}, m_Queue(queueSize) {}

ThreadPool::~ThreadPool() {
    while(m_IsRunning)
    {
        Stop();
    }
}

void ThreadPool::Start(int threadNum) {
    m_IsRunning = true;
    m_Threads.resize(threadNum);

    for(std::thread & work_thread: m_Threads)
    {
        work_thread = std::thread{&ThreadPool::PopAndExecuteTask, this};
        YLOG_INFO("启动线程池线程<%lu>", CastThreadIDToInt(work_thread.get_id()))
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

} // yy::util