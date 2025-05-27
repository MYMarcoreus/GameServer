#ifndef GAMESERVER_STDOUTLOGAPEENDER_H
#define GAMESERVER_STDOUTLOGAPEENDER_H

#include "ILogAppender.h"

namespace yy::Ylog {


/// @brief 日志输出至标准输出
class StdoutLogApeender final : public ILogAppender {
public:
    StdoutLogApeender() = delete;

    explicit StdoutLogApeender(const std::string &format_pattern, int buffer_size = 4 * 1024, std::chrono::milliseconds flush_interval = 100ms);

    ~StdoutLogApeender() override = default;

    /// @brief 将日志信息msg写到标准输出
    void WriteLog(const std::shared_ptr<LogMessage> &msg) override;

    void AppendBuffer(const std::shared_ptr<LogMessage> & msg) override;

    void FlushBuffer() override;

private:
    LogBufferManager::BufferPtr m_newBuffer1;
    LogBufferManager::BufferPtr m_newBuffer2;
    LogBufferManager::BufferVector m_buffersToWrite;
};

}

#endif //GAMESERVER_STDOUTLOGAPEENDER_H
