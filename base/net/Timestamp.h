#ifndef LINUXGAMESERVER_TIMESTAMP_H
#define LINUXGAMESERVER_TIMESTAMP_H

#include <cstdint>
#include <string>
#include <chrono>
using namespace std::chrono_literals;

namespace yy::net {

using Microseconds = std::chrono::microseconds ;
using Seconds = std::chrono::seconds;

///@brief 保存并管理始于epoch的毫秒数，是一个时间点
class Timestamp {
public:
    explicit Timestamp(Microseconds  microSecondSinceEpoch = 0us);
    explicit Timestamp(timespec spec);

    ///Region 转换函数
    ///@brief 返回`秒.微秒`的字符串格式
    std::string ToString();

    /// @brief 返回格式化时间，默认格式为 2023-02-04 20:29:44.961172
    std::string ToFormattedString(const std::string &fmt = "%Y-%m-%d %H:%M:%S.", bool is_UTC = false);

    struct timespec ToTimespec();

    struct timeval ToTimeval();
    ///End 转换函数

    //Region GETTER
    Microseconds GetMircoSecondSinceEpoch() { return us_SinceEpoch_; }
    Seconds      GetSecondPart()            { return Seconds{us_SinceEpoch_ / k10_6}; }
    Microseconds GetMicroSecondPart()       { return us_SinceEpoch_ % k10_6; }
    bool   IsValid() { return us_SinceEpoch_ > 0us; }
    //End GETTER

    //Region SETTER
    void SetNow() { us_SinceEpoch_ = Timestamp::Now().GetMircoSecondSinceEpoch(); }
    void SetUnixTime(Seconds secondPart, Microseconds microSecondPart)
    { us_SinceEpoch_ = FromUnixTime(secondPart, microSecondPart).GetMircoSecondSinceEpoch(); }
    //End SETTER

    ///@brief 获得现在的时间戳
    static Timestamp Now();

    ///@brief 从`secondPart.microSecondPart`的时间格式构造Timestamp
    static Timestamp FromUnixTime(Seconds secondPart, Microseconds  microSecondPart);
private:
    constexpr static const Microseconds  k10_6 = 1000000us; // mircosecond == μs == 10^{-6} s
    Microseconds  us_SinceEpoch_; // Microsecond Since Epoch
};

///@brief 求未来的时间点
inline Timestamp operator+(Timestamp a, Microseconds b)
{ return  Timestamp(a.GetMircoSecondSinceEpoch() + b); }

///@brief 求时间段
inline Microseconds  operator-(Timestamp a, Timestamp b)
{ return a.GetMircoSecondSinceEpoch() - b.GetMircoSecondSinceEpoch(); }


///@brief 求过去的时间点
inline Timestamp operator-(Timestamp a, Microseconds  b)
{ return  Timestamp(a.GetMircoSecondSinceEpoch() - b); }

inline bool operator<(Timestamp a, Timestamp  b)
{ return a.GetMircoSecondSinceEpoch() < b.GetMircoSecondSinceEpoch(); }

inline bool operator>(Timestamp a, Timestamp  b)
{ return a.GetMircoSecondSinceEpoch() > b.GetMircoSecondSinceEpoch(); }

inline bool operator==(Timestamp a, Timestamp b)
{ return a.GetMircoSecondSinceEpoch() == b.GetMircoSecondSinceEpoch(); }


}

#endif //LINUXGAMESERVER_TIMESTAMP_H
