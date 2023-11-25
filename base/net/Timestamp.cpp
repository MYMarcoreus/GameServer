#include "Timestamp.h"
#include <ctime>

#include "cross_platform_defines.h"


namespace yy::net {

static_assert(sizeof(Timestamp) == sizeof(time_t),
              "Timestamp should be same size as time_t");



Timestamp Timestamp::Now() {
    auto now = std::chrono::system_clock::now();
    auto now_us = std::chrono::time_point_cast<std::chrono::microseconds>(now);
    return Timestamp{  now_us.time_since_epoch() };
}

Timestamp::Timestamp(Microseconds  microSecondSinceEpoch)
    : us_SinceEpoch_{microSecondSinceEpoch.count()} {}

Timestamp::Timestamp(timespec spec)
    : us_SinceEpoch_(spec.tv_sec * (Microseconds::period::den / Seconds::period::den) + spec.tv_nsec / (std::nano::den / std::micro::den) )
{ }

std::string Timestamp::ToString() {
    char buf[64]{0};
    snprintf(buf, 63, "%lld.%lld", GetSecondPart().count(), GetMicroSecondPart().count());
    return buf;
}

std::string Timestamp::ToFormattedString(const std::string &fmt, bool is_UTC) {

    // 将始于epoch的秒数转换为年月日时分
    struct tm now_tm{};
    time_t sec = GetSecondPart().count();
    time_t usec = GetMicroSecondPart().count();

#ifdef ____LINUX
    if(is_UTC) {
        // 使用UTC时间，全球所有地方都相同的一个时间
        gmtime_r(&sec, &now_tm);
    } else {
        // 使用当地时间，如东八区之类的时区时间
        localtime_r(&sec, &now_tm);
    }
#endif

#ifdef ____WINDOWS
    if(is_UTC) {
        // 使用UTC时间，全球所有地方都相同的一个时间
        gmtime_s(&now_tm, &sec);
    } else {
        // 使用当地时间，如东八区之类的时区时间
        localtime_s(&now_tm, &sec);
    }
#endif

    char buf[64]{0};
    size_t nByte = strftime(buf, 24, fmt.c_str(), &now_tm);

    // 加上微秒
    snprintf(buf + nByte, 8, "%06lld", usec);

    return buf;

}

Timestamp Timestamp::FromUnixTime(Seconds secondPart, Microseconds microSecondPart) {
    return Timestamp( Microseconds(secondPart.count() * k10_6.count() + microSecondPart.count()));
}

struct timespec Timestamp::ToTimespec() {
#ifdef ____LINUX
    return {GetSecondPart().count(), GetMicroSecondPart().count() * 1000};
#endif
#ifdef ____WINDOWS
    return {GetSecondPart().count(), static_cast<long>(GetMicroSecondPart().count() * 1000)};
#endif
}



}
