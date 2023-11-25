#include "RBTreeTimerManager.h"
#include "log.h"
#include "EventLoop.h"
#include "Timer.h"
#include "status/Status.h"

#ifdef ____LINUX
#include <sys/timerfd.h>
#endif

#ifdef ____WINDOWS
#  include <windows.h>
#endif

namespace yy::net {

using namespace yy::util;

namespace detail {

#ifdef ____LINUX

//! 内部类，用于管理Linux定时器文件描述符
struct __TimerfdManager
{
    // friend class ::yy::net::TimerManager;
public:
    ///@param cb 定时器到期回调
    __TimerfdManager(EventLoop * owner_loop, std::function<void()> cb);
    ~__TimerfdManager();

    void ReadTimerfd();
    void ResetTimerfd(Timestamp expireTime);

private:
    int  CreateTimerfd();

private:
    /* * Linux的特有的定时器，使用文件描述符的读写事件来通知定时器到期。
       * 当定时器到期时，用户可使用::read从定时器文件描述符读取定时器到期信息，read缓冲区类型是uint64_t */
    int     m_LinuxTimerFD;
    Channel m_LinuxTimerChannel;

    const Microseconds  kMinInterval = 100us;
};


///@brief 创建Linux定时器文件描述符
__TimerfdManager::__TimerfdManager(EventLoop * owner_loop, std::function<void()> cb):
        m_LinuxTimerFD(CreateTimerfd()),
        m_LinuxTimerChannel(owner_loop, m_LinuxTimerFD, "Linux Timerfd Channel")
{
    m_LinuxTimerChannel.SetReadCallback(cb);
    m_LinuxTimerChannel.EnableReading();
}

__TimerfdManager::~__TimerfdManager() {
    this->m_LinuxTimerChannel.DisableAllEvent();
    this->m_LinuxTimerChannel.RemoveFromLoop();
    ::close(m_LinuxTimerFD);
}


int __TimerfdManager::CreateTimerfd()
{
    /*
     * @param clock_id
     *      CLOCK_REALTIME    表示实际的时间，适合表示和计算实际的日期和时间
     *      CLOCK_MONOTONIC   表示时间间隔，保持单调增，适合做定时器
     *
     * @param flags
     *      TFD_CLOEXEC     为底层的文件描述符启用close-on-exec标志；和O_CLOEXEC一样，使用该标志可省去用fcntl进行设置；
     *                      这个标志告诉操作系统，在执行一个新的程序（通常是通过 exec 函数族执行）时是否自动关闭文件描述符。
     *      TFD_NONBLOCK    为底层的文件描述符启用 O_NONBLOCK 标志，使得定时器到期时用户的::read是非阻塞的
     * */
    int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timerfd < 0) {
        YLOG_FATAL("::timerfd_create error")
    }
    return timerfd;
}

void __TimerfdManager::ResetTimerfd(Timestamp expireTime)
{
    /*
     * struct itimerspec
      {
        struct timespec it_interval; // 第一次到期时间
        struct timespec it_value;    // 之后的到期时间即每隔多长时间到期
      };
     * */
    struct itimerspec newValue{};
    struct itimerspec oldValue{};

    //! 定时器间隔不小于100ns
    Microseconds interval = expireTime - Timestamp::Now();
    newValue.it_value = DurationToTimespec(interval < kMinInterval ? kMinInterval : interval);
    // YLOG_TRACE("newValue：{%ld, %ld}", newValue.it_value.tv_sec, newValue.it_value.tv_nsec)
    /*
     * @param flags
     *     0                   将newValue.it_value视为相对于调用::timerfd_settime时间点的相对时间
     *     TFD_TIMER_ABSTIME   将newValue.it_value视为绝对时间（从时钟的0点开始）
     * */
    int ret = ::timerfd_settime(m_LinuxTimerFD, 0, &newValue, &oldValue);
    if(ret < 0) {
        if(errno == EINVAL) {
            YLOG_ERROR("::timerfd_settime() error, {}, {}, {}", util::StatusCode{errno}.ToString().c_str(),
                       interval.count() / 10e6, TimespecToDuration(newValue.it_value).count());
        }
        else {
            YLOG_ERROR("::timerfd_settime() error, {}", util::StatusCode{errno}.ToString().c_str())
        }
    }

    memset(&newValue, 0, sizeof newValue);
    ::timerfd_gettime(m_LinuxTimerFD, &newValue);
    // YLOG_INFO("下次到期的定时器：{%ld, %ld}, {%ld, %ld}， 现在时间为 %ld",
    //     newValue.it_value.tv_sec, newValue.it_value.tv_nsec,
    //     newValue.it_interval.tv_sec, newValue.it_interval.tv_sec,
    //     Timestamp::Now().GetMircoSecondSinceEpoch().count()
    // );

    // YLOG_TRACE("重启定时器，下一次到期将在 %ld μs后", interval.count() )
}

void __TimerfdManager::ReadTimerfd()
{
    uint64_t buf;
    ssize_t n = ::read(m_LinuxTimerFD, &buf, sizeof buf);
    if(n != sizeof buf) {
        YLOG_ERROR("read(timerfd) error: except read {} byte, but only read {} byte", sizeof buf, n)
    }
}


#endif


} // detail








using namespace yy::net::detail;

RBTreeTimerManager::RBTreeTimerManager(EventLoop *owner_loop)
        : TimerManager(owner_loop)
        ,m_OwnerLoop(owner_loop)
        ,m_IsCallingExpiredTimers{false}
#ifdef ____LINUX
        ,m_TimerfdManager( std::make_unique<__TimerfdManager>(owner_loop, [this](){ this->HandleExpiredTimersCallback(); }) )
#endif

{

}

RBTreeTimerManager::~RBTreeTimerManager() {
    //TODO
}

Timestamp RBTreeTimerManager::GetEarliestExpiredTimeInLoop() {
    m_OwnerLoop->AssertInLoopingThread();
    if(m_TimerList.empty()) {
        return Timestamp{};
    }

    return (*m_TimerList.begin())->GetExpireTime();
}

TimerID RBTreeTimerManager::AddTimer(F_TaskCallback cb, Timestamp expiredTime, Microseconds interval) {
    TimerPtr timer = std::make_shared<Timer>(m_TimerCounter++, std::move(cb), expiredTime, interval);
    m_OwnerLoop->RunCallbackInLoop([this, timer](){RBTreeTimerManager::AddTimerInLoop(timer);});
    return timer->GetID();
}

void RBTreeTimerManager::AddTimerInLoop(TimerPtr timer) {
    m_OwnerLoop->AssertInLoopingThread();

    auto isEarliestExpiredTimerChanged = InsertTimer(timer);

    // YLOG_TRACE("已将Timer<%ld>加入定时器列表中，目前有%zu个定时器，其回调函数为<%s>，%s需要重置下一个到期的定时器",
    //            timer->GetID(), m_TimerList.size(),
    //            GetDemangleName(timer->m_Callback.target_type().m_TypeName()).c_str(),
    //            isEarliestExpiredTimerChanged ? "" : "不"
    // )

#ifdef ____LINUX
    if(isEarliestExpiredTimerChanged) {
        m_TimerfdManager->ResetTimerfd(timer->GetExpireTime());
    }
#endif
}

bool RBTreeTimerManager::InsertTimer(TimerPtr timer) {
    auto earliestExpiredTimer = GetEarliestExpriredTimer();

    bool isEarliestExpiredTimerChanged = false;
    if( earliestExpiredTimer and timer->GetExpireTime() < earliestExpiredTimer->GetExpireTime())
    {
        isEarliestExpiredTimerChanged = true;
    }
    else if(m_TimerList.size() == 0)
    {
        isEarliestExpiredTimerChanged = true;
    }
    m_TimerList.insert(timer);
    m_TimeridMap.insert(std::pair{timer->GetID(), timer});

    return isEarliestExpiredTimerChanged;
}

TimerPtr RBTreeTimerManager::GetEarliestExpriredTimer() {
    auto it = m_TimerList.begin();
    return  it!=m_TimerList.end() ? (*it) : nullptr;
}



void RBTreeTimerManager::CancelTimer(TimerID timerid) {
    m_OwnerLoop->RunCallbackInLoop([this, timerid]{ RBTreeTimerManager::CancelTimerInLoop(timerid); });
}

void RBTreeTimerManager::CancelTimerInLoop(TimerID timerid) {
    //! 按照ID来查找，如果查找到有效的timer，那么将其从定时器列表中删除，
    auto it = m_TimeridMap.find(timerid);
    TimerPtr timer = nullptr;
    if(it != m_TimeridMap.end()) {
        timer = it->second;
        if(timer) {
            //! 从定时器列表中删除timer
            m_TimerList.erase(timer);
            m_TimeridMap.erase(timerid);
        }
    }

    //! 如果正在执行回调函数（说明用户在timer的回调函数中调用了CancelTimerInLoop进行了自cancel），那么便不能立马释放它，应延迟释放之。
    //! 需要等待它执行完释放，因此将其智能指针存入cancel列表，保留一个引用。
    if(timer and m_IsCallingExpiredTimers) { //! 如果m_IsCallingExpiredTimers为true，那么必有it == m_TimeridMap.end()
        m_CancelingTimerList.insert({timerid, timer});
    }
}




//! Linux系统timerfd的计时器到时的回调函数
/*! CancelTimerInLoop是pending函数，晚于HandleExpiredTimers这个读回调函数执行，所以在执行HandleExpiredTimers时
  ! 不会有未处理的已cancel timer
  ! */
int RBTreeTimerManager::HandleExpiredTimersInLoop() {
    m_OwnerLoop->AssertInLoopingThread();
    YLOG_TRACE("定时器到期，处理定时器！")

    //! 获取到期的timer，将这些timer从定时器列表中删除
    auto expiredTimers = GetExpiredTimers();
    int expiredCount = expiredTimers.size();
    if(expiredCount == 0 )
        return 0;

    //! 执行到期的timer的回调函数，使用m_IsCallingExpiredTimers和m_CancelingTimerList来防止在回调函数中cancel定时器自身。
    m_IsCallingExpiredTimers = true;
    for(TimerPtr & expiredTimer: expiredTimers) {
        /* timer的引用计数：expired列表有一个，如果在下面函数有调用cancel，那么cancel列表则会有一个引用计数 */
        expiredTimer->ExecuteCallback();
        //! cancel列表是expired列表的子集，因为cancel列表是expired timer在执行回调函数时调用了cancel才变成了cancel timer
    }
    m_IsCallingExpiredTimers = false;

    //! 定时器可能在expiredTimer->ExecuteCallback()处cancel自身，因此cancel的策略只是将其加入cance列表中，
    //! 等到timer的回调函数执行完后再来释放其资源
    ResetAndFreeExpiredTimers(expiredTimers);

    return expiredCount;
}

void RBTreeTimerManager::HandleExpiredTimersCallback()
{
#ifdef ____LINUX
    m_TimerfdManager->ReadTimerfd();
#endif

    HandleExpiredTimersInLoop();
}

std::vector<TimerPtr> RBTreeTimerManager::GetExpiredTimers() {
    /* ****** 从定时器列表中删除过期的Timer，并将其存入expired列表中 ****** */
    decltype(m_TimerList)::iterator bound_end;
    {
        //! 找到expired timer分界点
        TimerPtr nowTimer = std::make_shared<Timer>(m_TimerCounter++, nullptr, Timestamp::Now());
        bound_end = m_TimerList.lower_bound(nowTimer);
    }
    std::vector<TimerPtr> expiredTimers;
    std::copy(m_TimerList.begin(), bound_end, std::back_inserter(expiredTimers));

    //! 因为这里能知道要删除的范围，所以在这里删除比较好一点，否则可以在ResetAndFreeExpiredTimers中统一删除，而非在这里删除
    m_TimerList.erase(m_TimerList.begin(), bound_end);
    for(auto & expiredTimer: expiredTimers) {
        m_TimeridMap.erase(expiredTimer->GetID());
    }

    return expiredTimers;
}

void RBTreeTimerManager::ResetAndFreeExpiredTimers(std::vector<TimerPtr> &expiredTimers) {
    //! 处理expired timer和cancel timer
    for(TimerPtr & expiredTimer: expiredTimers) {
        auto it_cancel = m_CancelingTimerList.find(expiredTimer->GetID());
        //! 如果定时器循环执行且也没被cancel，那么将其重启加入到定时器列表中
        if(expiredTimer->IsRepeated() and it_cancel == m_CancelingTimerList.end())
        {
            expiredTimer->Restart();
            InsertTimer(expiredTimer);
        }
        //! 如果定时器不循环执行 或 定时器被加入了cancel列表，那就释放该定时器所占的空间
        else {
            TimerPtr & cancelTimer = it_cancel->second;
            //! 释放timer的最后两个引用计数：到期定时器列表 和 cancel定时器列表 中的引用计数
            expiredTimer.reset();
            if(it_cancel != m_CancelingTimerList.end() and cancelTimer) {
                cancelTimer.reset();
            }
        }
    }

    //! 重置Linux定时器为下一个计时器到期的时间
#ifdef ____LINUX
    auto earliestExpriredTimer = GetEarliestExpriredTimer();
    if(earliestExpriredTimer) {
        m_TimerfdManager->ResetTimerfd(earliestExpriredTimer->GetExpireTime());
    }
#endif

    //! 清空cancel列表
    m_CancelingTimerList.clear();
}




bool RBTreeTimerManager::TimerComparator::operator()(const TimerPtr &a, const TimerPtr &b) const {
    //! 如果有一个指针为空，则按照原生指针的地址来比较
    return (!a or !b) ? a.get() < b.get() : *a < *b;
}



} // yy::net
