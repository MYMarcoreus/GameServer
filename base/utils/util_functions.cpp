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
#include <random>
#include <string>
#include <stduuid/uuid.h>

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

    const std::string base_name_str(base_name);
    return base_name_str.substr(0, base_name_str.find_last_of('/'));
}


std::string get_current_fmt_time(const std::string & fmt, const bool need_us)
{
    return make_format_time(get_current_time(), fmt, need_us);
}

std::chrono::system_clock::time_point get_current_time()
{
    return std::chrono::system_clock::now();
}

std::string make_format_time(const std::chrono::system_clock::time_point now, const std::string& fmt, const bool need_us)
{
    const auto now_us = std::chrono::time_point_cast<std::chrono::microseconds>(now);
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

    char buf[128]{};
    const auto nBytes = std::strftime(buf, sizeof(buf), fmt.c_str(), &now_tm);

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
    const int old_option = fcntl(sockfd, F_GETFL);
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
void set_reuseaddr(const SocketApiWrapper::socket_t sockfd, const bool onoff)
{
    const int opt_val = onoff;
    ::setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt_val, sizeof(opt_val));
}

// 设置close()关闭连接的行为，启用后在超时时间timeout到达时会直接异常终止连接使得无需进行四次挥手和TIMEWAIT
void set_linger(const int sockfd, const bool onoff, const int timeout)
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
    if (os_fd == -1 || (void*)os_fd == INVALID_HANDLE_VALUE) {
        return false;
    }

    // 可选：进一步判断句柄是否有效
    DWORD flags = 0;
    if (GetHandleInformation(reinterpret_cast<HANDLE>(os_fd), &flags) == 0) {
        return false;
    }

    return true;
#endif
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
    if (std::is_same_v<decltype(ret), int>) {
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

std::string GetStrThreadID() {
    //! 线程安全，稍稍会慢一点点，占用空间也会多一点点，不过使用简单
    const std::thread::id threadId = std::this_thread::get_id();
    std::ostringstream oss;
    oss << threadId;
    return oss.str();
}

std::string CastThreadIDToStr(const std::thread::id threadId) {
    std::ostringstream oss;
    oss << threadId;
    return oss.str();
}

size_t GetHashThreadID() {
    const std::thread::id threadId = std::this_thread::get_id();
    static std::hash<std::thread::id> hasher;
    return hasher(threadId);
}

size_t CastThreadIDToHash(const std::thread::id threadId) {
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

std::chrono::nanoseconds TimespecToDuration(const struct timespec spec) {
    const std::chrono::nanoseconds t = std::chrono::seconds{spec.tv_sec} + std::chrono::nanoseconds{spec.tv_nsec};
    return  t;
}



std::string GetLastErrorInfo() {

#ifdef  ____WINDOWS
    return GetErrorInfo(GetLastError());
#endif

#ifdef ____LINUX
    return GetErrorInfo(errno);
#endif
}



std::string GetErrorInfo(int64_t err) {
#ifdef  ____WINDOWS
    LPVOID errorMsg;
    FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
            nullptr,
            (DWORD)err,
            0, // Default language
            reinterpret_cast<LPSTR>(&errorMsg),
            0,
            nullptr
    );

    return (char *)errorMsg;
#endif

#ifdef ____LINUX
    const auto p = strerrorname_np(err);
    return std::string( p ? p : "" ) + "(" + StrError((int)err) + ")";
#endif
}



std::string GenerateTokenOld(const size_t length) {
    static constexpr char charset[] =
        "0123456789"
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    thread_local std::mt19937 gen{std::random_device{}()};
    thread_local std::uniform_int_distribution<> dis(0, sizeof(charset) - 2);

    std::string token;
    token.reserve(length);
    for (size_t i = 0; i < length; ++i)
        token += charset[dis(gen)];
    return token;
}

std::string GenerateToken()
{
    thread_local std::mt19937 engine{std::random_device{}()};
    thread_local uuids::uuid_random_generator gen{&engine};
    const uuids::uuid uuid = gen();
    return uuids::to_string(uuid);
}


uint8_t GenerateXorCode()
{
    std::mt19937  eng{std::random_device{}() }; // 真随机数
    static std::uniform_int_distribution<int> dis(1, 125); // [1, 125]
    const uint8_t gen_val = static_cast<uint8_t>(dis(eng));
    return gen_val;
}
}
