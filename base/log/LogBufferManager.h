#pragma once

#include <condition_variable>
#include <functional>


namespace yy::util
{
class LinearBuffer;
}

namespace yy::Ylog
{
class LogMessage;
class ILogAppender;

class LogBufferManager {
public:
    using BufferPtr = std::unique_ptr<util::LinearBuffer>;
    using BufferVector = std::vector<BufferPtr>;

    explicit LogBufferManager(int bufferSize = 40960, std::function<void()> cb = nullptr);
    ~LogBufferManager();

    /// 向当前缓冲区追加日志字符串
    void Append(const std::string & logstr);

    /// 交出当前缓冲区中的数据，并替换为 newBuffer1, newBuffer2
    void Swap(BufferPtr& outBuffer1, BufferPtr& outBuffer2, BufferVector& outBuffersToWrite, std::chrono::milliseconds flush_interval);

private:
    size_t m_bufferSize;

    BufferPtr m_current{}; //! 前端缓冲区
    BufferPtr m_next{};    //! 备用缓冲区

    BufferVector m_buffersToWrite;

    std::mutex m_mutex;
    std::condition_variable m_isEmpty;

    std::function<void()> m_writeCb;
};

}

