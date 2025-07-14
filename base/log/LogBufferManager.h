#pragma once

#include <condition_variable>

#include "SequentialBuffer.h"

namespace yy::Ylog
{
class LogMessage;
class ILogAppender;

class LogBufferManager {
public:
    using BufferPtr = std::unique_ptr<util::SequentialBuffer>;
    using BufferVector = std::vector<BufferPtr>;

    explicit LogBufferManager(int bufferSize = 40960);

    /// 向当前缓冲区追加日志字符串
    void Append(const std::string & logstr);

    /// 交出当前缓冲区中的数据，并替换为 newBuffer1, newBuffer2
    void Swap(BufferPtr& outBuffer1, BufferPtr& outBuffer2, BufferVector& outBuffersToWrite, std::chrono::milliseconds flush_interval);

private:
    size_t m_bufferSize;

    BufferPtr m_current;
    BufferPtr m_next;

    BufferVector m_buffersToWrite;

    std::mutex m_mutex;
    std::condition_variable m_isEmpty;
};

}

