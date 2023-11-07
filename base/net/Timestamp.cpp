#include "Timestamp.h"
#include <sys/time.h> // gettimeofday
#include <ctime>


namespace yy::net {

static_assert(sizeof(Timestamp) == sizeof(time_t),
              "Timestamp should be same size as time_t");



Timestamp Timestamp::Now() {
    struct timeval now{};
    gettimeofday(&now, nullptr);
    return Timestamp{ Microseconds(now.tv_usec + now.tv_sec * k10_6.count())  };
}

Timestamp::Timestamp(Microseconds  microSecondSinceEpoch)
    : us_SinceEpoch_{microSecondSinceEpoch.count()} {}

Timestamp::Timestamp(timespec spec)
    : us_SinceEpoch_(spec.tv_sec * (Microseconds::period::den / Seconds::period::den) + spec.tv_nsec / (std::nano::den / std::micro::den) )
{ }

std::string Timestamp::ToString() {
    char buf[64]{0};
    snprintf(buf, 63, "%ld.%ld", GetSecondPart().count(), GetMicroSecondPart().count());
    return buf;
}

std::string Timestamp::ToFormattedString(const std::string &fmt, bool is_UTC) {
    struct timeval tvTime{};
    tvTime.tv_sec  = GetSecondPart().count();
    tvTime.tv_usec = GetMicroSecondPart().count();

    // 将始于epoch的秒数转换为年月日时分
    struct tm now_tm{};
    if(is_UTC) {
        // 使用UTC时间，全球所有地方都相同的一个时间
        gmtime_r(&tvTime.tv_sec, &now_tm);
    } else {
        // 使用当地时间，如东八区之类的时区时间
        localtime_r(&tvTime.tv_sec, &now_tm);
    }

    char buf[64]{0};
    size_t nByte = strftime(buf, 24, fmt.c_str(), &now_tm);

    // 加上微秒
    snprintf(buf + nByte, 8, "%06ld", tvTime.tv_usec);

    return buf;

}

Timestamp Timestamp::FromUnixTime(Seconds secondPart, Microseconds microSecondPart) {
    return Timestamp( Microseconds(secondPart.count() * k10_6.count() + microSecondPart.count()));
}




}