#ifndef GAMESERVER_ILOGAPPENDER_H
#define GAMESERVER_ILOGAPPENDER_H

#include "LogFormatter.h"

#include <mutex>
#include <memory>
#include <string>

#include "LogBufferManager.h"
using namespace std::chrono_literals;


namespace yy::Ylog {

class LogMessage;

/// @brief 日志添加器(基类)：用于写一条日志，至于写到哪，这由子类的实现决定
class ILogAppender {
public:
    using ptr = std::shared_ptr<ILogAppender>;

    explicit ILogAppender(std::string format_pattern, const int buffer_size = 2 * 1024, const std::chrono::milliseconds flush_interval = 500ms)
        : m_BuffferSize(buffer_size), m_FlushInterval(flush_interval), m_bufferManager{buffer_size}, m_formatter(std::move(format_pattern))
    { }

    virtual ~ILogAppender() = default;

    /// @brief 同步写一条日志
    virtual void WriteLog(const std::shared_ptr<LogMessage> &msg) = 0;

    /// @brief 异步写一条日志
    virtual void AppendBuffer(const std::shared_ptr<LogMessage> & msg) = 0;

    virtual void FlushBuffer() = 0;


    /// @brief 读取配置文件时，配置文件中能够指定单个Appender时间项的格式
    void SetTimeFormat(const std::string &timeFmtPattern = "%Y-%m-%d %H:%M:%S.", const bool need_us = true)
    {
        m_formatter.SetTimeFormat(timeFmtPattern, need_us);
    }

    [[nodiscard]] std::chrono::milliseconds get_flush_interval() const
    {
        return m_FlushInterval;
    }

    [[nodiscard]] std::chrono::high_resolution_clock::time_point get_last_flush_time() const
    {
        return m_lastFlushTime;
    }

protected:
    const int                           m_BuffferSize;
    const std::chrono::milliseconds     m_FlushInterval;
    std::chrono::high_resolution_clock::time_point m_lastFlushTime = std::chrono::high_resolution_clock::now();
    LogBufferManager m_bufferManager;

    mutable std::mutex  m_mutex;     // 多个logger输出时进行互斥(测试表明：似乎不用上锁也行)
    LogFormatter        m_formatter;
};


}
#endif //GAMESERVER_ILOGAPPENDER_H
