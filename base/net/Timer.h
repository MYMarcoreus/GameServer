#ifndef LINUXGAMESERVER_TIMER_H
#define LINUXGAMESERVER_TIMER_H

#include"Timestamp.h"
#include<cstdint>
#include<functional>
#include<memory>
#include<atomic>

#include"net_definations.h"

namespace yy::net {

class Timer: public util::noncopyable
{
    friend class TimerManager;
public:
    // 一般来说expiredTime = Timestamp::Now() + internalTime
    Timer(F_TimerCallback timerCallback, Timestamp expiredTime, Microseconds  internalTime = 0us);

    ///@brief 直接执行Timer回调函数
    void ExecuteCallback();

    ///@brief 重置到期时间
    void Restart() { m_ExpireTime = m_IsRepeat ? Timestamp::Now()+m_Interval : Timestamp{0us} ; };

    //Region GETTER
    auto      GetInterval()   const { return m_Interval  ; }
    Timestamp GetExpireTime() const { return m_ExpireTime; }
    TimerID   GetID()         const { return m_CreateSequence; }
    bool      IsRepeated()    const { return m_IsRepeat  ; }
    //End GETTER


private:

    Microseconds      m_Interval;   // 时间间隔
    F_TimerCallback   m_Callback;   // 定时器到期时执行的函数
    bool              m_IsRepeat;   // 定时器到期时是否再加入定时器列表（循环执行）
    Timestamp         m_ExpireTime; // 定时器到期时间
    TimerID           m_CreateSequence;

    static std::atomic<TimerID>  m_TimerCounter;
};

}


#endif //LINUXGAMESERVER_TIMER_H
