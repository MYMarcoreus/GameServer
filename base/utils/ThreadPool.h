#pragma once
#include "BoundedLockedQueue.hpp"
#include "net_definations.h"
#include <vector>
#include <thread>
#include <functional>



namespace yy::net {

class EventLoop;

class ThreadPool {
public:
    using Task = F_TaskCallback; //! 必须是`std::function<void ()>`，否则lambda函数/std::bind函数无法传入带捕获列表/多参数函数

public:
    ///@param name 线程池的用途
    ///@param queueSize 任务队列的长度，默认为无限队列（其实为int的最大值）
    explicit ThreadPool(const std::string&  name, int queueSize = util::BoundedLockedQueue<Task>::NO_BOUND_SIZE);

    ~ThreadPool();

    ///@brief 启动所有线程，但是并没有任务，所以一开始会wait任务
    void Start(EventLoop * timerloop, int threadNum = static_cast<int>(std::thread::hardware_concurrency()) / 2);

    ///@brief 不会立即停止所有线程，而是等待它们将任务队列中的余下任务完成后再停止
    void Stop();

    // void SetUpdate(std::chrono::microseconds internal);

    ///@brief 加入一个任务到任务队列中
    void PushTask(Task task);

    ///@brief 线程池大小
    int ThreadPoolSize() const { return m_Threads.size();  }

    ///@brief 队列目前的长度
    int TaskQueueSize() const { return m_Queue.size(); }


    std::optional<TimerID> RunTaskAt(Timestamp time, Task cb);
    std::optional<TimerID> RunTaskAfter(Microseconds delay, Task cb);
    std::optional<TimerID> RunTaskEvery(Microseconds interval, Task cb);
    void CancelTimer(TimerID timerid);


private:
    ///@brief 每个线程所执行的函数
    void PopAndExecuteTask();

private:
    std::string                         m_name;
    util::BoundedLockedQueue<Task>      m_Queue;     // 任务队列
    std::vector<std::thread>            m_Threads;   // 管理线程
    bool                                m_IsRunning;
    EventLoop *                         m_TimerLoop;
    std::vector<TimerID>                m_runningTimerIDs;
};




} // yy:: util
