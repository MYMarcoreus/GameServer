#ifndef GAMESERVER_FILELOGAPPENDER_H
#define GAMESERVER_FILELOGAPPENDER_H

#include <condition_variable>

#include "ILogAppender.h"
#include "LogFormatter.h"
#include "cross_platform_defines.h"

namespace yy::Ylog {

/// @brief 日志输出至文件
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


#endif //GAMESERVER_FILELOGAPPENDER_H
