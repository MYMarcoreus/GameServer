#ifndef GAMESERVER_TIMERMANAGER_H
#define GAMESERVER_TIMERMANAGER_H

#include "net_definations.h"
#include <atomic>

namespace yy {
namespace net {

class TimerManager {
public:
    TimerManager(EventLoop * loop): m_loop{loop}, m_TimerCounter{0} {}
    ~TimerManager() {};

    ///@brief 在定时器列表中新建一个定时器
    virtual TimerID AddTimer(F_TaskCallback cb, Timestamp expiredTime, Microseconds  interval = 0us) = 0;

    ///@brief 按照定时器id来取消定时器
    virtual void CancelTimer(TimerID timerid) = 0;


    ///@brief 计时器到时的回调函数
    virtual int HandleExpiredTimersInLoop() = 0;

    virtual Timestamp GetEarliestExpiredTimeInLoop() = 0;


    static TimerManager* NewDefaultTimerManager(EventLoop * loop);

protected:
    EventLoop *           m_loop;
    std::atomic<TimerID>  m_TimerCounter;
};

} // yy
} // net

#endif //GAMESERVER_TIMERMANAGER_H
