#include "Timer.h"
#include "log.h"

namespace yy::net {

using namespace yy::util;

std::atomic<TimerID>  Timer::m_TimerCounter = 0;

Timer::Timer(F_TimerCallback timerCallback, Timestamp expiredTime, Microseconds  internalTime)
        : m_Callback(timerCallback),
          m_ExpireTime(expiredTime),
          m_IsRepeat(internalTime.count() > 0),
          m_Interval(internalTime),
          m_CreateSequence(m_TimerCounter++)
{}

void Timer::ExecuteCallback() {
    if(m_Callback) {
        YLOG_TRACE("执行定时器回调<%s>！", GetDemangleName(m_Callback.target_type().name()).c_str())
        m_Callback();
    }
}


} // yy::net