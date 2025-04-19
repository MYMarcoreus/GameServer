#ifndef GAMESERVER_PRIORITYQUEUETIMERMANAGER_H
#define GAMESERVER_PRIORITYQUEUETIMERMANAGER_H


#include <vector>
#include <unordered_map>
#include <queue>
#include <cstdint>
#include <memory>
#include <functional>

#include "Timestamp.h"
#include "net_definations.h"
#include "TimerManager.h"


namespace yy::net {


// timer scheduler implemented by priority queue(min-heap)
//
// complexity:
//     StartTimer  CancelTimer   PerTick
//      O(log N)    O(1)       O(1)
//

//! 只属于一个线程
class PriorityQueueTimerManager : public TimerManager
{
public:
    PriorityQueueTimerManager(EventLoop * loop);
    virtual ~PriorityQueueTimerManager() override;

    virtual TimerID AddTimer(F_TaskCallback cb, Timestamp expiredTime, Microseconds  interval = 0us) override;

    virtual void CancelTimer(TimerID timer_id) override;

    virtual int HandleExpiredTimersInLoop() override;

    virtual Timestamp GetEarliestExpiredTimeInLoop() override;
private:

    ///@brief 在AddTimer()中传递给m_OwnerLoop->RunCallbackInLoop()的回调函数，为EventLoop中的pending函数
    void AddTimerInLoop(Timer * timer);

    ///@brief 在CancelTimer()中传递给m_OwnerLoop->RunCallbackInLoop()的回调函数，为EventLoop中的pending函数
    void CancelTimerInLoop(TimerID);

    //! 使用优先队列管理Timer。
    //!     如果存储的是Timer类，则可以不传入外置的比较函数，在Timer类中定义比较运算符即可；
    //!     但是这里存储的是Timer*指针，Timer类内的比较运算符只能定义Timer和其它对象的比较，无法定义Timer*自身的比较，因此需定义比较运算符
    struct TimerComparator {
        bool operator()(const Timer * a, const Timer * b) const;
    };
    using TimersContainer = std::priority_queue<Timer*, std::vector<Timer*>, TimerComparator>;

private:
    TimersContainer                 m_timers;  // binary timer heap
    std::unordered_map<int, Timer*> m_timersref;     // to make O(1) lookup
    // int                                 next_id_;
};




} // yy::util

#endif //GAMESERVER_PRIORITYQUEUETIMERMANAGER_H
