#ifndef LINUXGAMESERVER_EVENTLOOP_H
#define LINUXGAMESERVER_EVENTLOOP_H

#include <vector>
#include <memory>
#include <thread>
#include <functional>
#include <mutex>

#include "Timestamp.h"
#include "net_definations.h"


namespace yy::net {

class Channel;
class Poller;
class TimerManager;
class Timer;
class WakeupManager;

//! 循环执行IO
class EventLoop {
public:
    using ChanneList = std::vector<Channel *>;
    using F_PendingCallback = std::function<void()>;
    using PendingCallbackList = std::vector<F_PendingCallback>;
    using F_CloseSocketsCallback = std::function<void()>;

public:
    EventLoop(Milliseconds defaultPollwaitTimeout);
    ~EventLoop();

    /*! 核心函数 */
    void Loop();

    /*! 向m_quitManager所管理的Channel发送数据，m_quitManager收到数据后 !*/
    void QuitLoop();

    //Region 代办函数相关
    /*! 在loop执行过程中可执行外部传入的函数（可能是本IO线程，也可能是其他线程），然后在EventLoop所在的线程执行，从而保证线程安全 !*/
    ///@brief 如果在本线程，则直接执行functor；如果在其他线程，则将functor加入代办函数列表
    void RunCallbackInLoop(F_PendingCallback cb);
    ///@brief 将functor加入代办函数列表
    void EnqueueCallbackInLoop(F_PendingCallback cb);
    //End 代办函数相关

    //Region 调用Poller的对应函数
    void UpdateChannel(Channel * channel);
    void RemoveChannel(Channel * channel);
    bool    HasChannel(Channel * channel);
    //End 调用Poller的对应函数

    //Region 定时器相关函数
    ///@brief 在某个时间点time执行回调函数cb
    TimerID RunAt(Timestamp time, F_TaskCallback cb);
    ///@brief 在delay微妙后执行回调函数cb
    TimerID RunAfter(Microseconds delay, F_TaskCallback cb);
    ///@brief 以interval的时间间隔循环执行回调函数cb
    TimerID RunEvery(Microseconds interval, F_TaskCallback cb);
    ///@brief 撤销id为timerid的定时器
    void CancelTimer(TimerID timerid);
    //End 定时器相关函数

    ///@brief 保证一定是EventLoop的创建者在执行
    void AssertInLoopingThread(const std::string & filepath = __FILE__, int fileline = __LINE__);

    ///@brief 判断是否运行在EventLoop所在线程
    bool IsInLoopingThread() { return std::this_thread::get_id() == m_ThreadID; }

    std::thread::id GetThreadID() { return m_ThreadID; }

    ///@brief 获取当前线程的EventLoop
    static EventLoop * GetEventLoopOfThisThread();

    bool IsLooping() const { return m_IsLooping; }

    void SetCloseSocketsCallback(F_CloseSocketsCallback cb) { m_CloseSocketsCallback = cb; }

    ///@brief 唤醒正在阻塞在PollWait的EventLoop线程，以处理代办函数
    // void Wakeup();
private:

    ///@brief 在EventLoop::Loop()每一轮循环的最后执行代办函数列表
    void CallPenddingCallbacks();

    //! epoll_wait只支持ms不支持us
    Milliseconds GetPollwaitTimeout();


private:
    //! 请注意成员的初始化顺序是按照这里声明的顺序（内存排布的顺序），
    //! ThreadID需要在最开始声明，因为AssertInLoopingThread()需要用到它
    //! 而TimerManager和WakeupManager依赖Poller，
    //! 所以需要调整成员声明的顺序！
    std::thread::id                m_ThreadID;
    bool                           m_IsLooping;
    bool                           m_IsQuit;
    std::unique_ptr<Poller>        m_Poller;          //! 其实EventLoop有一些函数都是直接调用Poller的函数，所以Poller需要先初始化
    std::unique_ptr<TimerManager>  m_TimerManager;
    std::unique_ptr<WakeupManager> m_WakeupManager;   // 用于QuitLoop，唤醒正在Loop()阻塞的PollWait()函数
    ChanneList                     m_ActiveChannels;
    Milliseconds                   m_DefaultPollwaitTimeout;

    PendingCallbackList  m_PenddingFunctors;      // 在每一轮Loop最后调用的代办函数列表
    std::mutex           m_PenddingFunctorsMutex; // 保护代办函数列表
    std::atomic_bool     m_IsCallingPenddingFunctors;

    F_CloseSocketsCallback m_CloseSocketsCallback;
};

}

#endif //LINUXGAMESERVER_EVENTLOOP_H

