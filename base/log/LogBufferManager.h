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
    using WriteCallback = std::function<void(BufferVector &)>;
    using FlushCallback = std::function<void()>;

    explicit LogBufferManager(int bufferSize = 40960, WriteCallback write_cb = nullptr, FlushCallback flush_cb = nullptr);
    ~LogBufferManager();

    /// 向当前缓冲区追加日志字符串
    void Append(const std::string & logstr);

    /// 交出当前缓冲区中的数据，并替换为 tempBuffer1_, tempBuffer2_，然后写入目的地
    void SwapAndWriteFlush(std::chrono::milliseconds flush_interval);

private:
    size_t m_bufferSize;

    BufferPtr m_current; //! 前端缓冲区
    BufferPtr m_next;    //! 备用缓冲区
    BufferVector m_buffersToWrite; //! 待写缓冲区

    BufferPtr tempBuffer1_;
    BufferPtr tempBuffer2_;
    BufferVector tempBuffersToWrite_;

    WriteCallback writeCb_;
    FlushCallback m_flushCb;


    std::mutex m_mutex;
    std::condition_variable m_isEmpty;

};

}

