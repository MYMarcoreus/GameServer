#include "EventLoop.h"
#include "Poller.h"
#include "Channel.h"
#include "log.h"
#include "util_functions.h"
#include "TimerManager.h"
#include "SocketApiWrapper.h"

#ifdef ____LINUX
#include <sys/eventfd.h>
#endif

#ifdef ____WINDOWS
#include "socket_definations.h"
#endif

namespace yy::net {

using namespace yy::util;

namespace {
thread_local EventLoop *____EventLoopInThisThread = nullptr;

static SocketApiWrapper::socket_t CreatEventFD() {
#ifdef ____LINUX
    //! 相比使用管道，::eventfd更加高效
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0) {
        YLOG_FATAL("Failed in eventfd")
    }
    return evtfd;
#endif

#ifdef ____WINDOWS
    int sockfd = SocketApiWrapper::create_or_die();
    return sockfd;
#endif
}


}




class WakeupManager
{
public:
    WakeupManager(EventLoop * loop);

    ~WakeupManager();

    int GetFD() { return wakeupEventFD_; }

    void Write();

private:
    void Read();

    int wakeupEventFD_;
    std::unique_ptr<Channel> wakeupChannel_;
};


WakeupManager::WakeupManager(EventLoop *loop)
        : wakeupEventFD_(CreatEventFD()),
          wakeupChannel_(std::make_unique<Channel>(loop, wakeupEventFD_, "Wakeup Eventfd Channel"))
{
    wakeupChannel_->SetReadCallback([this](){ this->Read(); });
    wakeupChannel_->EnableReading();
}

WakeupManager::~WakeupManager() {
    this->wakeupChannel_->DisableAllEvent();
    this->wakeupChannel_->RemoveFromLoop();

    SocketApiWrapper::close(wakeupEventFD_);
}

void WakeupManager::Read() {
    uint64_t msg = 1;
    auto ret = SocketApiWrapper::recv(wakeupEventFD_, &msg, sizeof msg, 0);
    if(ret < 0) {
        YLOG_ERROR("EventLoop::WakeupManager::Read() ::read() error: %s", ::yy::util::StrError(errno).c_str())
    }
}

void WakeupManager::Write() {
    //! 向唤醒事件文件描述符进行写，以触发其epoll事件
    uint64_t msg = 1;
    auto ret = SocketApiWrapper::send(wakeupEventFD_, &msg, sizeof msg, 0);
    if(ret < 0) {
        YLOG_ERROR("EventLoop::WakeupManager::Write ::write() error: %s", ::yy::util::StrError(errno).c_str())
    }
}






EventLoop::EventLoop(bool useETIfEpoller) :
        m_Poller(Poller::NewDefaultPoller(this)),     // many channel fd
        m_TimerManager{new TimerManager(this)}, // timerfd
        m_ThreadID(std::this_thread::get_id()),
        m_IsLooping(false),
        m_IsQuit(false),
        m_IsCallingPenddingFunctors(false),
        m_WakeupManager(std::make_unique<WakeupManager>(this))                             // wakefd
{
    if(____EventLoopInThisThread) {
        YLOG_FATAL("There is already a EventLoop Object in this thread<%lu>!", ::yy::util::GetIntThreadID())
    } else {
        ____EventLoopInThisThread = this;
    }
}

EventLoop::~EventLoop() {
    AssertInLoopingThread();

    while(m_IsLooping) {
        QuitLoop();
    }
}

void EventLoop::UpdateChannel(Channel * channel) {
    AssertInLoopingThread();
    m_Poller->UpdateChannel(channel);
}

void EventLoop::RemoveChannel(Channel *channel) {
    AssertInLoopingThread();
    m_Poller->RemoveChannel(channel);
}

bool EventLoop::HasChannel(Channel *channel) {
    return m_Poller->HasChannel(channel);
}


void EventLoop::AssertInLoopingThread() {
    if(!IsInLoopingThread()) {
        YLOG_FATAL("EventLoop Created In thread<%lu>, but now in %lu",
            ::yy::util::CastThreadIDToInt(m_ThreadID), ::yy::util::GetIntThreadID())
    }
}

void EventLoop::Loop() {
    AssertInLoopingThread();

    m_IsLooping = true;

    while(!m_IsQuit)
    {
        m_ActiveChannels.clear();

        YLOG_TRACE("Before PollWait();")

        // 等待事件发生，由Poller填充ActiveChannels
        m_Poller->PollWait(m_ActiveChannels);

        //todo 对发生的事件进行优先级排序

        YLOG_TRACE("\nAfter PollWait(), 发生了%zu个事件", m_ActiveChannels.size())

        // 处理发生了事件的channel
        for(Channel * activeChannel: m_ActiveChannels) {
            activeChannel->HandleHappenedEvent();
        }
        YLOG_TRACE("Before CallPenddingCallbacks();")

        // 运行代办函数
        CallPenddingCallbacks();

        YLOG_TRACE("After CallPenddingCallbacks();")

        //! 检查被shutdown的套接字，看是否到达关闭的要求，若到达，关闭之。
        if(m_CloseSocketsCallback)
            m_CloseSocketsCallback();
    }

    m_IsLooping = false;

}

void EventLoop::QuitLoop() {
    m_IsQuit = true;

    /* 若在当前线程执行QuitLoop()，则不用Wakeup()，因为当前线程只有可能在`active_event->HandleHappenedEvent();`处执行各种函数，
    执行完后会到下一轮循环然后检查m_IsQuit然后退出循环 */
    if(!IsInLoopingThread()){
        Wakeup();
    }
}

void EventLoop::Wakeup() {
    m_WakeupManager->Write();
}

EventLoop *EventLoop::GetEventLoopOfThisThread() {
    return ____EventLoopInThisThread;
}

void EventLoop::RunCallbackInLoop(F_PendingCallback cb) {
    //! 如果是EventLoop所在线程调用，则直接执行；如果在其他线程，则将函数放入代办函数列表中。
    if(IsInLoopingThread()) {
        YLOG_TRACE("直接执行代办函数<%s>", GetDemangleName(cb.target_type().name()).c_str())
        cb();
    } else {
        EnqueueCallbackInLoop(cb);
    }
}

void EventLoop::EnqueueCallbackInLoop(F_PendingCallback cb) {
    {
        std::lock_guard lg{m_PenddingFunctorsMutex};
        m_PenddingFunctors.emplace_back(cb);
    }

    /* * ①：如果是其它线程调用的：那么必须调用Wakeup()才能使得PollWait()返回来执行CallPenddingFunctors()
       * ②：如果是本线程调用的：
       *        ②①：如果未在执行CallPenddingFunctors()，那说明此时线程是在`active_event->HandleHappenedEvent()`
       *        中的各类回调函数中调用的EnqueueFunctorInLoop()，那么无需Wakeup，等待调用者执行完毕即可接着执行CallPenddingFunctors()
       *        ②②：如果正在执行CallPenddingFunctors()：那么在CallPenddingFunctors()执行完后，线程会阻塞在PollWait()而造成死锁，
       *        因此需要Wakeup来唤醒线程来执行下一次的CallPenddingFunctors()
     * */
    if(!IsInLoopingThread() or m_IsCallingPenddingFunctors) {
        Wakeup();
    }
    YLOG_TRACE("已将函数<%s>加入代办函数列表", GetDemangleName(cb.target_type().name()).c_str())
}

void EventLoop::CallPenddingCallbacks() {
    m_IsCallingPenddingFunctors = true;

    /* * 因为要执行代办函数列表中的所有函数，所以不能在上锁之后一个一个取出代办函数再执行，
       * 这样使得其它线程因执行EnqueueFunctorInLoop()而阻塞在互斥锁上，
       * 因此先将此刻的代办函数从列表中全部取出（使用swap方法），就能解决该问题了。
     * */
    std::vector<F_PendingCallback> callingFunctors;
    {
        std::lock_guard lg{m_PenddingFunctorsMutex};
        m_PenddingFunctors.swap(callingFunctors);
    }

    // YLOG_TRACE("即将执行代办函数！")

    for (const F_PendingCallback& functor: callingFunctors) {
        functor();
        YLOG_TRACE("执行代办函数<%s>！", GetDemangleName(functor.target_type().name()).c_str())
    }

    // YLOG_TRACE("代办函数执行完毕！")

    m_IsCallingPenddingFunctors = false;
}

TimerID EventLoop::RunAt(Timestamp time, F_TimerCallback cb) {
    return m_TimerManager->AddTimer(std::move(cb), time);
}

TimerID EventLoop::RunAfter(Microseconds delay, F_TimerCallback cb) {
    return RunAt(Timestamp::Now() + delay, std::move(cb));
}

TimerID EventLoop::RunEvery(Microseconds interval, F_TimerCallback cb) {
    return m_TimerManager->AddTimer(std::move(cb), Timestamp::Now() + interval, interval);
}

void EventLoop::CancelTimer(TimerID timerid) {
    m_TimerManager->CancelTimer(timerid);
}



}