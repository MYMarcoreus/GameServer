#ifndef GAMESERVER_ILOGAPPENDER_H
#define GAMESERVER_ILOGAPPENDER_H


#include <mutex>
#include <memory>
#include <string>

namespace yy::Ylog {


/// @brief 日志添加器(基类)：用于写一条日志，至于写到哪，这由子类的实现决定
class ILogAppender {
public:
    using ptr = std::shared_ptr<ILogAppender>;

public:
    virtual ~ILogAppender() = default;

    /// @brief 写一条日志
    virtual void WriteLog(const std::shared_ptr<class LogMessage> &msg) = 0;

    /// @brief 读取配置文件时，配置文件中能够指定单个Appender时间项的格式
    virtual void SetTimeFormat(const std::string &timeFmtPattern = "%Y-%m-%d %H:%M:%S.", bool need_us = true) = 0;

protected:

    mutable std::mutex m_mutex;     // 多个logger输出时进行互斥(测试表明：似乎不用上锁也行)
};


}
#endif //GAMESERVER_ILOGAPPENDER_H
