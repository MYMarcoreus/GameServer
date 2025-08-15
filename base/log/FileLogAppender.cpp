#include "FileLogAppender.h"
#include "log.h"
#include "cross_platform_defines.h"
#include "util_functions.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include "LogBufferManager.h"
#include "LinearBuffer.h"



namespace yy::Ylog {

//! 保留了多个线程写同一文件时会输出三次"---start---"和"---finish--"的“bug”，从而让你知道有多个线程在写同一文件
FileLogAppender::FileLogAppender(std::string logfilepath, const std::string& format_pattern,
const int buffer_size, const std::chrono::milliseconds flush_interval) :
    ILogAppender(format_pattern, buffer_size, flush_interval),
    m_logfilepath{std::move(logfilepath)},
    m_filefd(-1)
{
    m_filefd = OPEN(m_logfilepath.c_str(), O_CREAT | O_APPEND | O_WRONLY);
    assert(m_filefd != -1);

    const auto start_info = "---------------start---------------\n";
    auto ret = WRITE(m_filefd, start_info, strlen(start_info) );
}

FileLogAppender::~FileLogAppender()
{
    if(util::isOpenedFD(m_filefd)) {
        const auto start_info = "---------------finish---------------\n";
        auto ret = WRITE(m_filefd, start_info, strlen(start_info));
        FLUSH(m_filefd);
        CLOSE(m_filefd);
    }
}

// FIXME：并没有做到多个logger写同一个文件时的互斥
void FileLogAppender::WriteLog(const std::shared_ptr<LogMessage> & msg)
{
    assert(msg != nullptr);
// TICK_START()
    std::lock_guard lg{m_mutex};

    // 保证写日志的原子性，使得日志按照生成的时间输出到文件
    const std::string & msg_str = m_formatter->format(msg);

    // 使用O_APPEND模式打开的文件的write()是原子操作：保证这一条信息写到内核缓冲队列中（缺点，来一条日志就写一条，频繁陷入内核态）
    auto ret = WRITE(m_filefd, msg_str.c_str(), msg_str.size());
}

void FileLogAppender::Write(const BufferVector & outBuffersToWrite)
{
#ifdef ____LINUX
    if (outBuffersToWrite.size() > 0 and outBuffersToWrite[0]->GetDataSize() != 0) {
        // 写文件
        std::vector<IOV_TYPE> iovs;
        iovs.reserve(outBuffersToWrite.size());
        for (const auto& buffer : outBuffersToWrite) {
            iovs.emplace_back(const_cast<char*>(buffer->Peek()), buffer->GetDataSize());
        }
        auto ret = ::writev(m_filefd, iovs.data(), static_cast<int>(iovs.size()));
    }
#elif defined(____WINDOWS)
    if (outBuffersToWrite.size() > 0 and outBuffersToWrite[0]->GetDataSize() != 0) {
        for (const auto& buffer : outBuffersToWrite) {
            WRITE(m_filefd, buffer->Peek(), buffer->GetDataSize());
        }
    }
#else
#error Platform not supported
#endif
}

void FileLogAppender::Flush()
{
}


}
