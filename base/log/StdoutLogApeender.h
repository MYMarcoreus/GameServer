#pragma once

#include "ILogAppender.h"

namespace yy::Ylog {


/// @brief 日志输出至标准输出
class StdoutLogApeender final : public ILogAppender {
public:
    StdoutLogApeender() = delete;

    explicit StdoutLogApeender(const std::string &format_pattern, int buffer_size = 4 * 1024, std::chrono::milliseconds flush_interval = 1000ms);

    ~StdoutLogApeender() override;

    /// @brief 同步写一条日志
    void WriteLog(const std::shared_ptr<LogMessage> &msg) override;

private:
    /// @brief 异步写多条日志流
    void Write(const BufferVector & outBuffersToWrite) override;

    void Flush();
};

}

