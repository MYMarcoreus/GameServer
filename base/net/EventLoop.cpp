#include "EventLoop.h"
#include "Poller.h"
#include "IOChannel.h"
#include "log.h"
#include "TimerManager.h"
#include "SocketApiWrapper.h"

#ifdef ____WINDOWS
#include "socket_definations.h"
#include "IPAddress.h"
#endif

namespace yy::net {

using namespace yy::util;

namespace {
thread_local EventLoop *____EventLoopInThisThread = nullptr;

}



struct WakeupFD {
    WakeupFD() {
    #ifdef ____LINUX
        //! 相比使用管道，::eventfd更加高效
        int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (evtfd < 0) {
            throw std::system_error(errno, std::system_category(), "eventfd");
        }
        this->wait_fd = evtfd;
        this->notify_fd = evtfd;
    #elif defined(____WINDOWS)
        InitWakeUpWithUCP();
    #else
        #error Platform not supported
    #endif
    }

    void InitWakeUpWithUCP() {
        this->wait_fd =  SocketApiWrapper::create_tcp_or_die(false);
        auto loopback_addr = std::make_shared<IPv4Address>("127.0.0.1");
        SocketApiWrapper::bind_or_die(this->wait_fd, loopback_addr);
        this->wait_addr = SocketApiWrapper::GetLocalAddr(this->wait_fd);

        this->notify_fd = SocketApiWrapper::create_udp_or_die(true);
    }

    ~WakeupFD() {
        if (this->wait_fd == this->notify_fd) {
            SocketApiWrapper::close(this->wait_fd);
        } else {
            SocketApiWrapper::close(this->wait_fd);
            SocketApiWrapper::close(this->notify_fd);
        }
    }

    SocketApiWrapper::socket_t wait_fd;
    SocketApiWrapper::socket_t notify_fd;
    IPAddressPtr wait_addr;
};



class WakeupManager
{
public:
    WakeupManager(EventLoop * loop);

    ~WakeupManager();


    void NotifyIfNeed() {
        if(isNeedWakeup) {
            Notify();
        }
    }

    void SetNeedWake(bool val) { isNeedWakeup = val; }

private:
    void OnNotify();
    void Notify();

    WakeupFD wakeupEventFD_;
    std::unique_ptr<IOChannel> wakeupChannel_;
    std::atomic<bool> isNeedWakeup;
};


WakeupManager::WakeupManager(EventLoop *loop)
        : wakeupEventFD_{},
          wakeupChannel_(std::make_unique<IOChannel>(loop, wakeupEventFD_.wait_fd, "Wakeup Eventfd Channel")),
          isNeedWakeup{false}
{
    wakeupChannel_->SetReadCallback([this](){ this->OnNotify(); });
    wakeupChannel_->EnableReading();
}

WakeupManager::~WakeupManager() {
    this->wakeupChannel_->ResetAndRemoveFromPoller();
}

void WakeupManager::OnNotify() {
    isNeedWakeup = false;

    uint64_t msg = 1;
#ifdef ____LINUX
    auto ret = ::read(wakeupEventFD_.read_fd, &msg, sizeof msg);
    if(ret < 0) {
        YLOG_ERROR("EventLoop::WakeupManager::Read() ::read() error: {}", GetLastErrorInfo())
    }
#elif defined(____WINDOWS)
    // auto rst = SocketApiWrapper::recv(wakeupEventFD_.wait_fd, &msg, sizeof msg, 0);
    auto rst = SocketApiWrapper::recvfrom(
        wakeupEventFD_.wait_fd, &msg, sizeof msg, 0, nullptr);
    if(rst.HasError()) {
        YLOG_ERROR("EventLoop::WakeupManager::OnNotify() ::read() error: {}", GetLastErrorInfo())
    }
#else
    #error Platform not supported
#endif
}

//! 向唤醒事件文件描述符进行写，以触发其poll事件
void WakeupManager::Notify() {
    uint64_t msg = 1;
#ifdef ____LINUX
    auto ret = ::write(wakeupEventFD_.write_fd, &msg, sizeof msg);
    if(ret < 0) {
        YLOG_ERROR("EventLoop::WakeupManager::Write ::write() error: {}", GetLastErrorInfo())
    }
#elif defined(____WINDOWS)
    // auto rst = SocketApiWrapper::send(wakeupEventFD_.notify_fd, &msg, sizeof msg, 0);
    auto rst = SocketApiWrapper::sendto(
        wakeupEventFD_.notify_fd, &msg, sizeof msg, 0, wakeupEventFD_.wait_addr);

    if(rst.HasError()) {
        YLOG_ERROR("EventLoop::WakeupManager::Notify ::write() error: {}", GetLastErrorInfo());
    }
#else
    #error Platform not supported
#endif
}




EventLoop::EventLoop(Milliseconds defaultPollwaitTimeout)
        // 跨平台I/O多路复用（Linux使用epoll，Windows使用select）
        : m_Poller(Poller::NewDefaultPoller(this)) //* many channel fd
        //
        , m_TimerManager{TimerManager::NewDefaultTimerManager(this)} //* timerfd
        , m_ThreadID(std::this_thread::get_id())
        , m_IsLooping(false)
        , m_IsQuit(false)
        , m_IsCallingPenddingFunctors(false)
        , m_WakeupManager(std::make_unique<WakeupManager>(this)) //* wakefd
        , m_DefaultPollwaitTimeout(defaultPollwaitTimeout)
{
    if(____EventLoopInThisThread) {
        YLOG_FATAL("There is already a EventLoop Object in this thread<{}>!", ::yy::util::GetStrThreadID())
    } else {
        ____EventLoopInThisThread = this;
    }
}

EventLoop::~EventLoop() {
    AssertInLoopingThread(__FILE__, __LINE__);

    while(m_IsLooping) {
        QuitLoop();
    }
}

void EventLoop::UpdateChannel(IOChannel * channel) {
    AssertInLoopingThread();
    m_Poller->UpdateChannel(channel);
}

void EventLoop::RemoveChannel(IOChannel *channel) {
    AssertInLoopingThread(__FILE__, __LINE__);
    m_Poller->RemoveChannel(channel);
}

bool EventLoop::HasChannel(IOChannel *channel) {
    return m_Poller->HasChannel(channel);
}


void EventLoop::AssertInLoopingThread(const std::string & filepath, int fileline) {
    if(!IsInLoopingThread()) {
        YLOG_FATAL("[{}:{}]::EventLoop Created In thread<{}>, but now in {}", filepath, fileline,
            ::yy::util::CastThreadIDToStr(m_ThreadID), ::yy::util::GetStrThreadID())
    }
}

void EventLoop::Loop() {
    AssertInLoopingThread(__FILE__, __LINE__);

    m_IsLooping = true;

    Milliseconds timeout{};
    while(!m_IsQuit)
    {
        m_ActiveChannels.clear();

        YLOG_TRACE("Before PollWait();")

        //! 距离下一个定时器超时的时长（没有定时器就是距离默认超时时间的时长）
        timeout = GetPollwaitTimeout();

        //! Windows没有类似Linux的定时器，直接在这里处理已超时的定时器
        //! Linux则有内置定时器，RBTreeTimerManager内已将其作为Channel加入m_Poller的监听范围中，在PollWait内处理超时的定时器
#ifdef ____WINDOWS
        if(timeout <= 0ms) {
            m_TimerManager->HandleExpiredTimersInLoop();
            continue;
        }
#endif

        m_WakeupManager->NotifyIfNeed();

        //! 等待timeout ms，由Poller填充ActiveChannels
        m_Poller->PollWait(m_ActiveChannels, timeout);

        //todo 对发生的事件进行优先级排序
        YLOG_TRACE("\nAfter PollWait(), 发生了{}个事件", m_ActiveChannels.size())

        //! 处理发生了事件的channel
        for(IOChannel * activeChannel: m_ActiveChannels) {
            activeChannel->HandleHappenedEvent();
        }
        YLOG_TRACE("Before CallPenddingCallbacks();")

        //! 运行代办函数
        CallPenddingCallbacks();

        YLOG_TRACE("After CallPenddingCallbacks();")

        //! 检查被shutdown的套接字，看是否到达关闭的要求，若到达，关闭之。
        if(m_CloseSocketsCallback) {
            m_CloseSocketsCallback();
        }
    }

    m_IsLooping = false;

}

void EventLoop::QuitLoop() {
    m_IsQuit = true;

    /* 若在当前线程执行QuitLoop()，则不用Wakeup()，因为当前线程只有可能在`active_event->HandleHappenedEvent();`处执行各种函数，
    执行完后会到下一轮循环然后检查m_IsQuit然后退出循环 */
    if(!IsInLoopingThread()){
        m_WakeupManager->SetNeedWake(true);
    }
}


EventLoop *EventLoop::GetEventLoopOfThisThread() {
    return ____EventLoopInThisThread;
}

void EventLoop::RunCallbackInLoop(F_PendingCallback cb) {
    //! 如果是EventLoop所在线程调用，则直接执行；如果在其他线程，则将函数放入代办函数列表中。
    if(IsInLoopingThread()) {
        YLOG_TRACE("直接执行代办函数<{}>", GetDemangleName(cb.target_type().name()).c_str())
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
    YLOG_TRACE("已将函数<{}>加入代办函数列表", GetDemangleName(cb.target_type().name()).c_str())

    if(!IsInLoopingThread() or m_IsCallingPenddingFunctors) {
        m_WakeupManager->SetNeedWake(true);
    }
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

    for (const F_PendingCallback& functor: callingFunctors) {
        YLOG_TRACE("开始执行代办函数<{}>！", GetDemangleName(functor.target_type().name()).c_str())
        functor();
        YLOG_TRACE("执行完毕代办函数<{}>", GetDemangleName(functor.target_type().name()).c_str())
    }

    m_IsCallingPenddingFunctors = false;
}

TimerID EventLoop::RunAt(Timestamp time, F_TaskCallback cb) {
    return m_TimerManager->AddTimer(std::move(cb), time);
}

TimerID EventLoop::RunAfter(Microseconds delay, F_TaskCallback cb) {
    return RunAt(Timestamp::Now() + delay, std::move(cb));
}

TimerID EventLoop::RunEvery(Microseconds interval, F_TaskCallback cb) {
    return m_TimerManager->AddTimer(std::move(cb), Timestamp::Now() + interval, interval);
}

void EventLoop::CancelTimer(TimerID timerid) {
    m_TimerManager->CancelTimer(timerid);
}

Milliseconds EventLoop::GetPollwaitTimeout() {
    auto expiretime = m_TimerManager->GetEarliestExpiredTimeInLoop();

    Milliseconds timeout = m_DefaultPollwaitTimeout;
    if(expiretime.IsValid()) {
        Microseconds difftime = expiretime - Timestamp::Now();
        auto a = Milliseconds{difftime.count()/1000};
        auto b = m_DefaultPollwaitTimeout;
        timeout = (a<b)?a:b;
    }

    return timeout;
}


}
