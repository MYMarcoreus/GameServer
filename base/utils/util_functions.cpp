#include "util_functions.h"
#include "SocketApiWrapper.h"

#include <fcntl.h>

#include <thread>
#include <chrono>
#include <cassert>
#include <algorithm>
#include <cstring>
#include <ratio>
#include <sstream>
#include <ctime>
#include <stdexcept>
#include <system_error>

#ifdef ____LINUX
    #include <sys/socket.h>
    #include <sys/eventfd.h>
#endif
#ifdef ____WINDOWS
    #include <winsock2.h>
    // #include <ws2tcpip.h>
#endif




namespace yy::util {

// 获当前应用程序所在的工作目录，将其作为配置文件所在的目录（结尾不带斜杠）
std::string GetCWD()
{
    char base_name[1024]{}; // 以斜杠结尾
#ifdef ____LINUX
    auto ret = ::readlink("/proc/self/exe", base_name, sizeof(base_name));
    if (ret < 0) {
        throw std::system_error(errno, std::system_category(), "::readlink Error");
    }
#endif
#ifdef ____WINDOWS
    auto ret = ::GetCurrentDirectory(MAX_PATH, base_name);
    if (ret != 0) {
        throw std::system_error(errno, std::system_category(), "::GetCurrentDirectory occurred");
    }
#endif

    std::string base_name_str(base_name);
    return base_name_str.substr(0, base_name_str.find_last_of('/'));
}


std::string get_current_fmt_time(const std::string & fmt, bool need_us)
{
//     struct timeval now{};
//     ::gettimeofday(&now, nullptr); // 返回精确到微秒（10^{-6}s）的始于epoch的时间

    auto now = std::chrono::system_clock::now();
    auto now_us = std::chrono::time_point_cast<std::chrono::microseconds>(now);
    auto now_raw = std::chrono::system_clock::to_time_t(now);

    std::tm now_tm{};
#ifdef ____LINUX
    localtime_r(&now_raw, &now_tm);
    // ::gmtime_r(&now.tv_sec, &now_tm); // 将始于epoch的秒数转换为年月日时分秒
#endif
#ifdef ____WINDOWS
    localtime_s(&now_tm, &now_raw);
    // ::gmtime_s(&now_tm, (time_t*)&now.tv_sec); // 将始于epoch的秒数转换为年月日时分秒
#endif

    char buf[128]{0};
    auto nBytes = std::strftime(buf, sizeof(buf), fmt.c_str(), &now_tm);

    // 加上微秒
    if (need_us) {
        auto us_part = std::chrono::duration_cast<std::chrono::microseconds>(now_us.time_since_epoch()) % std::chrono::microseconds::period::den;
        std::snprintf(buf + nBytes, sizeof(buf) - nBytes, "%06lld", (long long)(us_part.count())  );
    }

    return buf;
}


// 设置套接字为非阻塞I/O
uint32_t set_nonblocking_fd(SocketApiWrapper::socket_t sockfd)
{
#ifdef ____LINUX
    int old_option = fcntl(sockfd, F_GETFL);
    fcntl(sockfd, F_SETFL, old_option | O_NONBLOCK);
    return old_option;
#endif

#ifdef ____WINDOWS
    u_long mode = 1;  // 非零表示启用非阻塞模式
    ::ioctlsocket(sockfd, FIONBIO, &mode);
    return mode;
#endif
}


//设置地址重用
void set_reuseaddr(SocketApiWrapper::socket_t sockfd, bool onoff)
{
    int opt_val = onoff;
    ::setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt_val, sizeof(opt_val));
}

// 设置close()关闭连接的行为，启用后在超时时间timeout到达时会直接异常终止连接使得无需进行四次挥手和TIMEWAIT
void set_linger(int sockfd, bool onoff, int timeout)
{
    struct linger ling{};
    ling.l_onoff = onoff;
    ling.l_linger = timeout;
    ::setsockopt(sockfd, SOL_SOCKET, SO_LINGER, (const char *)&ling, sizeof(ling));
}

bool isOpenedFD(int fd)
{

#ifdef ____LINUX
    return fd >= 0 and fcntl(fd, F_GETFD) != -1;
#endif

#ifdef ____WINDOWS
    auto os_fd = _get_osfhandle(fd);
    int flags = _setmode(os_fd, _O_BINARY); // 使用 _O_BINARY 标志来获取文件描述符标志
    if (flags == -1) {
        // 获取失败
        return false;
    }
    return true;
#endif
}


void set_signal_handler(int SIGXXXX, sighandler_t sighandler)
{
    ::signal(SIGXXXX, sighandler);
}

void set_signal_ignore(int SIGXXXX)
{
    set_signal_handler(SIGXXXX, SIG_IGN);
}

void set_signal_handler_default(int SIGXXXX)
{
    set_signal_handler(SIGXXXX, SIG_DFL);
}

std::string StrError(int errnum)
{
    const char * str;
    char buf[100]{ };

#ifdef ____WINDOWS
    int rc = ::strerror_s(buf, 100, errnum);
    buf[100 - 1] = '\0';  // guarantee NUL termination
    if (rc == 0 && ::strncmp(buf, "Unknown error", 13) == 0)
        *buf = '\0';
    str = buf;
#else
    auto ret = ::strerror_r(errnum, buf, 100);
    if (std::is_same<decltype(ret), int>::value) {
        // POXSI `strerror_r`: `ret` is `int`:
        if(ret) {
            ::snprintf(buf, sizeof buf, "Unknown error %d", errnum);
            str = buf;
        }
    } else {
        // GNU `strerror_r`: `ret` is `char *`:
        str = reinterpret_cast<const char*>(ret);
    }
#endif

    return str;
}

bool StrCmp_IgnoreCase(const char * str1, const char * str2)
{
    int i = 0, j = 0;
    while(str1[i] != '\0' and str2[j] != '\0')
    {
        if(str1[i] == str2[j]) {
            i++, j++;
        } else {
            return false;
        }
    }

    return true;
}

// std::thread::native_handle_type GetIntThreadID() {
//     //! 很好，因为std::thread::id类型只有一个数据成员且没有虚函数，可直接取地址获得内部的原生线程id成员
//     std::thread::id threadId = std::this_thread::get_id();
//     return *(std::thread::native_handle_type*)(&threadId);
// }
// std::thread::native_handle_type CastThreadIDToInt(std::thread::id threadId) {
//     return *(std::thread::native_handle_type*)(&threadId);
// }

std::string GetStrThreadID() {
    //! 线程安全，稍稍会慢一点点，占用空间也会多一点点，不过使用简单
    std::thread::id threadId = std::this_thread::get_id();
    std::ostringstream oss;
    oss << threadId;
    return oss.str();
}

std::string CastThreadIDToStr(std::thread::id threadId) {
    std::ostringstream oss;
    oss << threadId;
    return oss.str();
}

size_t GetHashThreadID() {
    std::thread::id threadId = std::this_thread::get_id();
    static std::hash<std::thread::id> hasher;
    return hasher(threadId);
}

size_t CastThreadIDToHash(std::thread::id threadId) {
    static std::hash<std::thread::id> hasher;
    return hasher(threadId);
}

std::string GetDemangleName(std::string_view mangled_name) {
#ifdef ____GNUC
    return abi::__cxa_demangle(mangled_name.data(), nullptr, nullptr, nullptr);
#else
    return mangled_name.data();
#endif
}

struct timespec DurationToTimespec(std::chrono::nanoseconds nanoDuration) {
#ifdef ____LINUX
    auto nst = nanoDuration.count() % std::nano::den;
#endif
#ifdef ____WINDOWS
    long nst = (long)(nanoDuration.count() % std::nano::den);
#endif

    //! NOTE: 注意timespec的ns部分不能超过1s，否则在传入系统函数中会报EINVAL参数错误。
    return timespec{nanoDuration.count() / std::nano::den , nst};
}

std::chrono::nanoseconds TimespecToDuration(struct timespec spec) {
    std::chrono::nanoseconds t = std::chrono::seconds{spec.tv_sec} + std::chrono::nanoseconds{spec.tv_nsec};
    return  t;
}

SocketApiWrapper::socket_t CreatEventFD() {
#ifdef ____LINUX
    //! 相比使用管道，::eventfd更加高效
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0) {
        throw std::system_error(errno, std::system_category(), "eventfd");
    }
    return evtfd;
#endif

#ifdef ____WINDOWS
    return SocketApiWrapper::create_tcp_or_die(true);
#endif
}

std::string GetLastErrorInfo() {

#ifdef  ____WINDOWS
    return GetErrorInfo(GetLastError());
#endif

#ifdef ____LINUX
    return GetErrorInfo(errno);
#endif


}

std::string GetErrorInfo(uint64_t error) {
#ifdef  ____WINDOWS
    LPVOID errorMsg;
    FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
            nullptr,
            (DWORD)error,
            0, // Default language
            reinterpret_cast<LPSTR>(&errorMsg),
            0,
            nullptr
    );

    return (char *)errorMsg;
#endif

#ifdef ____LINUX
    auto p = strerrorname_np(error);
    return std::string( p ? p : "" ) + "(" + StrError((int)error) + ")";
#endif
}


}
