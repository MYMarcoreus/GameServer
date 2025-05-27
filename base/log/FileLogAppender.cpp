#include "FileLogAppender.h"
#include "log.h"
#include "cross_platform_defines.h"
#include "util_functions.h"

#include <cassert>
#include <cstring>
#include <iostream>


namespace yy::Ylog {

//! 保留了多个线程写同一文件时会输出三次"---start---"和"---finish--"的“bug”，从而让你知道有多个线程在写同一文件
FileLogAppender::FileLogAppender(std::string logfilepath, const std::string& format_pattern,
const int buffer_size, const std::chrono::milliseconds flush_interval) :
    ILogAppender(format_pattern, buffer_size, flush_interval),
    m_logfilepath{std::move(logfilepath)},
    m_filefd(-1),
    m_newBuffer1(std::make_unique<util::Buffer>(m_BuffferSize)),
    m_newBuffer2(std::make_unique<util::Buffer>(m_BuffferSize)),
    m_buffersToWrite{}
{
    m_buffersToWrite.reserve(16);

    m_filefd = OPEN(m_logfilepath.c_str(), O_CREAT | O_APPEND | O_WRONLY);
    assert(m_filefd != -1);

    const auto start_info = "---------------start---------------\n";
    auto ret = WRITE(m_filefd, start_info, strlen(start_info) );
}

FileLogAppender::~FileLogAppender()
{
    if(util::isOpenedFD(m_filefd)) {
        const auto start_info = "---------------finish---------------\n";
        auto ret = WRITE(m_filefd, start_info, strlen(start_info) );
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
    const std::string & msg_str = m_formatter.format(msg);

    // 使用O_APPEND模式打开的文件的write()是原子操作：保证这一条信息写到内核缓冲队列中（缺点，来一条日志就写一条，频繁陷入内核态）
    auto ret = WRITE(m_filefd, msg_str.c_str(), msg_str.size());
}

void FileLogAppender::AppendBuffer(const std::shared_ptr<LogMessage>& msg)
{
    m_bufferManager.Append(m_formatter.format(msg));
}

void FileLogAppender::FlushBuffer()
{
    m_bufferManager.Swap(m_newBuffer1, m_newBuffer2, m_buffersToWrite, m_FlushInterval);

#ifdef ____LINUX
    if (m_buffersToWrite.size() > 0 and m_buffersToWrite[0]->GetDataSize() != 0) {
        // 写文件
        std::vector<IOV_TYPE> iovs;
        iovs.reserve(m_buffersToWrite.size());
        for (const auto& buffer : m_buffersToWrite) {
            iovs.push_back({const_cast<char*>(buffer->Peek()), buffer->GetDataSize()});
        }
        auto ret = ::writev(m_filefd, iovs.data(), static_cast<int>(iovs.size()));
    }
#elif defined(____WINDOWS)
    if (m_buffersToWrite.size() > 0 and m_buffersToWrite[0]->GetDataSize() != 0) {
        for (const auto& buffer : m_buffersToWrite) {
            WRITE(m_filefd, buffer->Peek(), buffer->GetDataSize());
        }
    }
#else
#error Platform not supported
#endif




    // 修改缓冲区截断逻辑
    if (m_buffersToWrite.size() > 2) {
        m_buffersToWrite.resize(2);
    }

    if (m_newBuffer1 == nullptr)
    {
        // assert(!m_buffersToWrite.empty());
        m_newBuffer1 = std::move(m_buffersToWrite.back());
        m_buffersToWrite.pop_back();
        m_newBuffer1->Reset();
    }

    if (m_newBuffer2 == nullptr)
    {
        // assert(!m_buffersToWrite.empty());
        m_newBuffer2 = std::move(m_buffersToWrite.back());
        m_buffersToWrite.pop_back();
        m_newBuffer2->Reset();
    }

    m_buffersToWrite.clear();
}
}
