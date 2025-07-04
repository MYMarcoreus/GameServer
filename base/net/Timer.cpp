#include "Timer.h"
#include "log.h"

namespace yy::net {

using namespace yy::util;


Timer::Timer(TimerID id, F_TaskCallback timerCallback, const Timestamp expiredTime, const Microseconds interval)
        : m_Interval(interval),
          m_Callback(std::move(timerCallback)),
          m_IsRepeat(interval.count() > 0),
          m_ExpireTime(expiredTime),
          m_CreateSequence(id),
          m_IsCanceled{false}
{}

void Timer::ExecuteCallback() {
    if(m_Callback) {
        YLOG_TRACE("执行定时器回调<{}>！", GetDemangleName(m_Callback.target_type().name()).c_str())
        m_Callback();
    }
}

bool Timer::operator<(const Timer & other) const {
    //! 如果有一个指针为空 或 到期时间相同，则按照原生指针的地址来比较
    if(this->GetExpireTime() == other.GetExpireTime()) {
        return m_CreateSequence < other.m_CreateSequence;
    }

    return this->GetExpireTime() < other.GetExpireTime();
}


} // yy::net
