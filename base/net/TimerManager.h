#ifndef LINUXGAMESERVER_TIMERMANAGER_H
#define LINUXGAMESERVER_TIMERMANAGER_H

#include "Timestamp.h"
#include "Channel.h"
#include "net_definations.h"

#include<set>
#include<map>
#include<vector>
#include <atomic>




namespace yy::net {

class EventLoop;
class TimerManager;

namespace detail {
struct __TimerfdManager;
}

class TimerManager {
public:
    TimerManager(EventLoop * owner_loop);
    ~TimerManager();

    // 不使用Timer的裸指针/裸对象，所以需要额外定义智能指针对象的比较函数，不能使用Timer::operator<
    struct TimerComparator
    {
        ///@brief 按照小于号来比较
        bool operator()(const TimerPtr & a, const TimerPtr & b) const;
    };

    ///@brief 在定时器列表中新建一个定时器
    TimerID AddTimer(F_TaskCallback cb, Timestamp expiredTime, Microseconds  interval = 0us);

    ///@brief 按照定时器id来取消定时器
    void CancelTimer(TimerID timerid);

private:
    //Region 核心函数
    ///@brief 在AddTimer()中传递给m_OwnerLoop->RunCallbackInLoop()的回调函数，为EventLoop中的pending函数
    void AddTimerInLoop(TimerPtr timer);

    ///@brief 在CancelTimer()中传递给m_OwnerLoop->RunCallbackInLoop()的回调函数，为EventLoop中的pending函数
    void CancelTimerInLoop(TimerID);

    ///@brief Linux系统timerfd的计时器到时的回调函数
    void HandleExpiredTimers();
    ///End 核心函数

    ///@brief 获取下一个到期的timer
    TimerPtr GetEarliestExpriredTimer();

    ///@return 如果timer在插入定时器列表后成为最早到期的定时器，那么返回true，否则返回false。
    bool InsertTimer(TimerPtr timer);

    ///@brief 返回到期的timer列表（expired列表），同时将timer从定时器列表中删除
    std::vector<TimerPtr> GetExpiredTimers();

    ///@brief 释放掉cancel timer；如果timer是循环timer，则重新加入定时器列表，否则同样释放之。
    void ResetAndFreeExpiredTimers(std::vector<TimerPtr> & expiredTimers );

private:

    EventLoop *                m_OwnerLoop;
    std::unique_ptr<detail::__TimerfdManager>   m_TimerfdManager;

    //! 无需考虑以下三个容器的互斥操作，因为cancelTimer和addTimer操作都被放入了EventLoop中，是单线程操作！
    std::set<TimerPtr, TimerComparator> m_TimerList;  // 只有key
    std::map<TimerID, TimerPtr>         m_TimeridMap;
    //! m_TimeridMap逻辑上是m_TimerList的副本，可以以id为索引进行查找。因此将m_TimerList和m_TimeridMap统称为“定时器列表”

    //! 用于解决在timer回调函数执行时自我cancel的行为带来的错误
    std::map<TimerID, TimerPtr> m_CancelingTimerList; //! 逻辑上是expired列表的子集
    std::atomic_bool            m_IsCallingExpiredTimers;
};

}

#endif //LINUXGAMESERVER_TIMERMANAGER_H

