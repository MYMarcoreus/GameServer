#pragma once

#include "ILogAppender.h"
#include "LogFormatter.h"

namespace yy::Ylog {

class FileLogAppender final : public ILogAppender
{
public:
    explicit FileLogAppender(std::string logfilepath, const std::string& format_pattern,
        int buffer_size = 40 * 1024, std::chrono::milliseconds flush_interval = 3s);

    ~FileLogAppender() override;

    /// @brief 同步写
    void WriteLog(const std::shared_ptr<LogMessage> & msg) override;

private:
    /// @brief 异步写：将缓冲区数据写到文件中
    void Write(const BufferVector & outBuffersToWrite) override;
    void Flush() override;

    std::string m_logfilepath; // 完整的文件路径
    int         m_filefd;
};

} // namespace yy::Ylog
