#pragma once
#include "Timestamp.h"
#include "IOChannel.h"
#include "net_definations.h"
#include "TimerManager.h"

#include<set>
#include<map>
#include<vector>
#include <atomic>




namespace yy::net {


#ifdef ____LINUX
namespace detail {
struct __TimerfdManager;
}
#endif


class EventLoop;

class RBTreeTimerManager final : public TimerManager {
public:
    explicit RBTreeTimerManager(EventLoop * owner_loop);
    virtual ~RBTreeTimerManager() override;

    ///@brief 在定时器列表中新建一个定时器
    TimerID AddTimer(F_TaskCallback cb, Timestamp expiredTime, Microseconds  interval = 0us) override;

    TimerID AddTimer(const TimerPtr& timer) override;

    TimerPtr CreateTimer(Timestamp expiredTime, Microseconds interval) override;

    ///@brief 按照定时器id来取消定时器
    void CancelTimer(TimerID timerid) override;

    ///@brief Linux系统timerfd的计时器到时的回调函数
    int HandleExpiredTimersInLoop() override;

    Timestamp GetEarliestExpiredTimeInLoop() override;

private:
    // 不使用Timer的裸对象，所以需要额外定义智能指针对象的比较函数，不能直接使用Timer::operator<
    struct TimerComparator
    {
        ///@brief 按照小于号来比较
        bool operator()(const TimerPtr & a, const TimerPtr & b) const;
    };

    //Region 插入Timer相关函数
    ///@brief 在AddTimer()中传递给m_OwnerLoop->RunCallbackInLoop()的回调函数，为EventLoop中的pending函数
    void AddTimerInLoop(const TimerPtr& timer);
    ///@return 如果timer在插入定时器列表后成为最早到期的定时器，那么返回true，否则返回false。
    bool InsertTimer(const TimerPtr& timer);
    ///@brief 获取下一个到期的timer
    TimerPtr GetEarliestExpriredTimer();
    ///End

    ///@brief 在CancelTimer()中传递给m_OwnerLoop->RunCallbackInLoop()的回调函数，为EventLoop中的pending函数
    void CancelTimerInLoop(TimerID);

    ///@brief 返回到期的timer列表（expired列表），同时将timer从定时器列表中删除
    std::vector<TimerPtr> PopExpiredTimers();

    ///@brief 释放掉cancel timer；如果timer是循环timer，则重新加入定时器列表，否则同样释放之。
    void ResetAndFreeExpiredTimers(std::vector<TimerPtr> & expiredTimers );

private:
    //! 无需考虑以下三个容器的互斥操作，因为cancelTimer和addTimer操作都被放入了EventLoop中，是单线程操作！
    std::set<TimerPtr, TimerComparator>   m_Timers;  // 只有key
    std::unordered_map<TimerID, TimerPtr> m_Timerid2Timer;
    //! m_TimeridMap逻辑上是m_TimerList的副本，可以以id为索引进行查找。因此将m_TimerList和m_TimeridMap统称为“定时器列表”

    //! 用于解决在timer回调函数执行时自我cancel的行为带来的错误
    std::unordered_map<TimerID, TimerPtr> m_CancelingTimerList; //! 逻辑上是expired列表的子集
    std::atomic_bool            m_IsCallingExpiredTimers;

#ifdef ____LINUX
    std::unique_ptr<detail::__TimerfdManager>   m_TimerfdManager;
#endif
};

}
