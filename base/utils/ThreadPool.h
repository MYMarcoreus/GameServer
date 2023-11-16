#ifndef ____THREADPOOL_H
#define ____THREADPOOL_H

#include "BoundedQueue.hpp"
#include "net_definations.h"
#include <vector>
#include <thread>
#include <functional>



namespace yy::net {

class TimerManager;
class EventLoop;

class ThreadPool {
public:
    using Task = net::F_TaskCallback; //! 必须是`std::function<void ()>`，否则lambda函数/std::bind函数无法传入带捕获列表/多参数函数

public:
    ///@param thread_num 线程池中的线程数量，默认为CPU的核心数
    ///@param queueSize 任务队列的长度，默认为无限队列（其实为int的最大值）
    explicit ThreadPool(int queueSize = util::BoundedQueue<Task>::NO_BOUND_SIZE);

    ~ThreadPool();

    ///@brief 启动所有线程，但是并没有任务，所以一开始会wait任务
    void Start(net::EventLoop * timerloop, int threadNum = (int) std::thread::hardware_concurrency() / 2);

    ///@brief 不会立即停止所有线程，而是等待它们将任务队列中的余下任务完成后再停止
    void Stop();

    // void SetUpdate(std::chrono::microseconds internal);

    ///@brief 加入一个任务到任务队列中
    void PushTask(Task task);

    ///@brief 线程池大小
    int ThreadPoolSize() { return m_Threads.size();  }

    ///@brief 队列目前的长度
    int TaskQueueSize() { return m_Queue.size(); }


    net::TimerID RunTaskAt(net::Timestamp time, Task cb);
    net::TimerID RunTaskAfter(net::Microseconds delay, Task cb);
    net::TimerID RunTaskEvery(net::Microseconds interval, Task cb);
    void CancelTimer(net::TimerID timerid);


private:
    ///@brief 每个线程所执行的函数
    void PopAndExecuteTask();

private:
    util::BoundedQueue<Task>                m_Queue;     // 任务队列
    std::vector<std::thread>                m_Threads;   // 管理线程
    bool                                    m_IsRunning;
    net::EventLoop *                        m_TimerLoop;
    // std::unique_ptr<yy::net::TimerManager>  m_TimerManager;
};




} // yy:: util

#endif //____THREADPOOL_H
