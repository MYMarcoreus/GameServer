#include "ILogAppender.h"
#include "LogFormatter.h"
#include "LogBufferManager.h"

namespace yy::Ylog
{
ILogAppender::ILogAppender(std::string format_pattern, const int buffer_size, const std::chrono::milliseconds flush_interval):
    m_BuffferSize(buffer_size),
    m_FlushInterval(flush_interval),
    m_bufferManager{std::make_unique<LogBufferManager>(buffer_size,
        [this](const BufferVector & buffers){ Write(buffers); },
        [this]() { Flush(); }
    )},
    m_formatter(std::make_unique<LogFormatter>(std::move(format_pattern)))
{ }

ILogAppender::~ILogAppender()
{
}

void ILogAppender::AppendBuffer(const std::shared_ptr<LogMessage>& msg)
{
    m_bufferManager->Append(m_formatter->format(msg));
}

void ILogAppender::WriteAndFlush()
{
    m_bufferManager->SwapAndWriteFlush(m_FlushInterval);
}

void ILogAppender::SetTimeFormat(const std::string& timeFmtPattern, const bool need_us)
{
    m_formatter->SetTimeFormat(timeFmtPattern, need_us);
}

std::chrono::milliseconds ILogAppender::get_flush_interval() const
{
    return m_FlushInterval;
}

std::chrono::high_resolution_clock::time_point ILogAppender::get_last_flush_time() const
{
    return m_lastFlushTime;
}

}
