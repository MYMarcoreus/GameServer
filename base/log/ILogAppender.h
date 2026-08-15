#pragma once

#include <functional>
#include <mutex>
#include <memory>
#include <string>
#include <vector>


namespace yy::util
{
class LinearBuffer;
}

using namespace std::chrono_literals;

namespace yy::Ylog {
class LogFormatter;
class LogBufferManager;
class LogMessage;

/// @brief 日志添加器(基类)：用于写一条日志，至于写到哪，这由子类的实现决定
class ILogAppender {
public:
    using ptr = std::shared_ptr<ILogAppender>;
    using BufferPtr = std::unique_ptr<util::LinearBuffer>;
    using BufferVector = std::vector<BufferPtr>;
    using WriteCallback = std::function<void(BufferVector &)>;
    using FlushCallback = std::function<void()>;

    explicit ILogAppender(std::string format_pattern, int buffer_size = 2 * 1024,
        std::chrono::milliseconds flush_interval = 5000ms);

    virtual ~ILogAppender();

    /// @brief 同步写一条日志
    virtual void WriteLog(const std::shared_ptr<LogMessage> &msg) = 0;

    /// @brief 异步写一条日志：写入到缓冲区中
    void AppendBuffer(const std::shared_ptr<LogMessage> & msg);

    /// @brief 异步写线程执行
    void WriteAndFlush();

    /// @brief 读取配置文件时，配置文件中能够指定单个Appender时间项的格式
    void SetTimeFormat(const std::string &timeFmtPattern = "%Y-%m-%d %H:%M:%S.", bool need_us = true);

    [[nodiscard]] auto get_flush_interval() const -> std::chrono::milliseconds;

    [[nodiscard]] auto get_last_flush_time() const -> std::chrono::high_resolution_clock::time_point;

protected:
    /// @brief 异步写：具体如何将缓冲区内的数据写到目的地
    virtual void Write(const BufferVector & outBuffersToWrite) = 0;
    virtual void Flush() = 0;

protected:
    const int                                       m_BuffferSize;
    const std::chrono::milliseconds                 m_FlushInterval;
    std::chrono::high_resolution_clock::time_point  m_lastFlushTime = std::chrono::high_resolution_clock::now();
    std::unique_ptr<LogBufferManager>               m_bufferManager;
    std::unique_ptr<LogFormatter>                   m_formatter;
    mutable std::mutex                              m_mutex; // 多个logger同步输出时进行互斥
};

}

