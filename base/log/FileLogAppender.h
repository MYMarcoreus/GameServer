#pragma once

#include <condition_variable>

#include "ILogAppender.h"
#include "LogFormatter.h"
#include "cross_platform_defines.h"

namespace yy::Ylog {
/**
 * @brief FileLogAppender类实现了将日志信息写入到指定文件的功能。该类继承自ILogAppender接口。
 * 通过构造函数可以设置日志文件的路径、日志格式模式、缓冲区大小以及刷新间隔时间。
 * 内部使用了两个缓冲区来存储日志数据，并定期或在需要时将这些数据写入磁盘上的日志文件。
 * 日志消息首先被添加到当前活动的缓冲区中，当达到一定条件（如缓冲区满或超过设定的时间间隔）时，会触发缓冲区内容的刷新操作，将日志数据从内存写入到文件中。
 */
class FileLogAppender final : public ILogAppender
{
public:
    explicit FileLogAppender(std::string logfilepath, const std::string& format_pattern,
        int buffer_size = 40 * 1024, std::chrono::milliseconds flush_interval = 5s);

    ~FileLogAppender() override;

    /// @brief 将日志信息msg写到文件
    void WriteLog(const std::shared_ptr<LogMessage> & msg) override;

    void AppendBuffer(const std::shared_ptr<LogMessage> & msg) override;

    void FlushBuffer() override;

private:
    std::string m_logfilepath; // 完整的文件路径
    int m_filefd;
    LogBufferManager::BufferPtr m_newBuffer1;
    LogBufferManager::BufferPtr m_newBuffer2;
    LogBufferManager::BufferVector m_buffersToWrite;
};

} // namespace yy::Ylog
