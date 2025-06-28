#ifndef ____UTIL_FUNCTION_H
#define ____UTIL_FUNCTION_H

#include <string>
#include <chrono>
#include <csignal>
#include <atomic>
#include <thread>
#include "socket_definations.h"

#ifdef ____GNUC
#include <cxxabi.h>
#endif




namespace yy::util {

using nano = std::nano;
using micro = std::micro;
using milli = std::milli;
using second = std::ratio<1>;
using minute = std::ratio<60>;
using hour = std::ratio<3600>;

using namespace std::chrono_literals;

// 获当前应用程序所在的工作目录(不带最后的/)
extern std::string GetCWD();

/// @brief 获取当前的格式化时间，默认格式为 2023-02-04 20:29:44.961172
extern std::string get_current_fmt_time(
        const std::string &fmt = "%Y-%m-%d %H:%M:%S", bool need_us = true);


/// @brief 用于打点计时，注意使用默认类型则需要补上空模板参数：Ticker<>
/// @tparam Period 计时单位
/// @tparam ReturnType 计时精度
template<typename Period = milli, typename ReturnType = double>
class Ticker
{
public:
    using Clock = std::chrono::high_resolution_clock;

    /// @brief 返回自epoch起到现在的时间
    static ReturnType tick()
    {
        using namespace std::chrono;
        constexpr time_point<Clock> epoch{};
        const time_point<Clock> now{Clock::now()};
        return duration_cast<duration<ReturnType, Period>>(now - epoch).count();
    }
};


template<class T>
const char* TypeToName() {
#ifdef ____GNUC
    static const char* s_name = abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, nullptr);
#else
    static const char* s_name = typeid(T).name();
#endif
    return s_name;
}




// 设置套接字为非阻塞I/O
extern uint32_t set_nonblocking_fd(SocketApiWrapper::socket_t fd);

//设置地址重用
extern void set_reuseaddr(SocketApiWrapper::socket_t sockfd, bool onoff);

// 设置close()关闭连接的行为，启用后在超时时间timeout到达时会直接异常终止连接使得无需进行四次挥手和TIMEWAIT
extern void set_linger(SocketApiWrapper::socket_t sockfd, bool onoff, int timeout);

// 判断是否为已打开文件的描述符
extern bool isOpenedFD(int fd);

///@brief strerror的线程安全的跨平台C++版本
extern std::string StrError(int errnum);

///@brief 两字符数组逐字符比较
extern bool StrCmp_IgnoreCase(const char * str1, const char * str2);

extern std::string                     GetStrThreadID();
extern size_t                          GetHashThreadID();

extern std::string                     CastThreadIDToStr(std::thread::id);
extern size_t                          CastThreadIDToHash(std::thread::id);

extern std::string GetDemangleName(std::string_view mangled_name);

extern struct timespec DurationToTimespec(std::chrono::nanoseconds nanoDuration);
extern std::chrono::nanoseconds TimespecToDuration(struct timespec spec);

extern std::string GetLastErrorInfo();
extern std::string GetErrorInfo(int64_t err);

}



/******************************* 测试计时用 *******************************/
static std::atomic<int32_t> ____cnt = 0;
static double ____cnt_time = 0;

#ifdef ____DEBUG
#define TICK_START() \
    yy::util::Ticker<> ticker; \
    auto t1 = ticker.tick();

#define TICK_END_CALC() \
    auto t2 = ticker.tick(); \
    ____cnt++; \
    ____cnt_time += (t2 - t1); \
    printf("{%lf}\n", ____cnt_time);

#define TICK_END_CALCAVG() \
    auto t2 = ticker.tick(); \
    ____cnt++; \
    ____cnt_time += (t2 - t1); \
    printf("{%7d: %lf}\n", ____cnt.load(), ____cnt_time / ____cnt);


#define INTERVAL_DO(interval , something ) \
{                                           \
    static auto t1 = time(nullptr);          \
    auto t2 = time(nullptr);                  \
    if( t2-t1 >= interval )                    \
    {                                           \
        t1 = t2;                                 \
        something ;                               \
    }                                              \
}
#else
#define TICK_START()
#define TICK_END_CALC()
#define TICK_END_CALCAVG()
#define INTERVAL_DO(interval , something ) { something }
#endif




#endif // !____UTIL_FUNCTION_H

