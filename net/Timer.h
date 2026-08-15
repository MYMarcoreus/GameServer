#pragma once
#include"Timestamp.h"
#include<functional>
#include "net_definations.h"
#include "noncopyable.h"

namespace yy::net {

class Timer: public util::noncopyable
{
    // friend class TimerManager;
public:
    // 一般来说expiredTime = Timestamp::Now() + internalTime
    Timer(TimerID id, F_TaskCallback timerCallback, Timestamp expiredTime, Microseconds  interval = 0us);

    bool operator<(const Timer & other) const;

    void SetCallback(F_TaskCallback cb) { m_Callback = std::move(cb); };

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

    void SetCanceled() { m_IsCanceled = true; }
    bool IsCanceled() const { return m_IsCanceled; }

private:

    Microseconds      m_Interval;   // 时间间隔
    F_TaskCallback    m_Callback;   // 定时器到期时执行的函数
    bool              m_IsRepeat;   // 定时器到期时是否再加入定时器列表（循环执行）
    Timestamp         m_ExpireTime; // 定时器到期时间
    TimerID           m_CreateSequence;
    bool              m_IsCanceled;



};

}
