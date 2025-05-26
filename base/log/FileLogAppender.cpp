#include "FileLogAppender.h"
#include "cross_platform_defines.h"

namespace yy::Ylog {

//! 保留了多个线程写同一文件时会输出三次"---start---"和"---finish--"的“bug”，从而让你知道有多个线程在写同一文件
FileLogAppender::FileLogAppender(const std::string& logfilepath, const std::string& format_pattern) //NOLINT
    : m_logfilepath{logfilepath},
    m_formatter(std::make_shared<LogFormatter>(format_pattern))
{
#if USE_CPP_STREAM
    if (m_ofs.is_open())
        m_ofs.close();
    m_ofs.open(m_logfilepath, std::ios::out | std::ios::app);
    m_ofs << "---------------start---------------\n";
#else
    m_filefd = OPEN(m_logfilepath.c_str(), O_CREAT | O_APPEND | O_WRONLY);
    assert(m_filefd != -1);

    const auto start_info = "---------------start---------------\n";
    auto ret = WRITE(m_filefd, start_info, strlen(start_info) );
#endif
}

FileLogAppender::~FileLogAppender()
{
#if USE_CPP_STREAM
    if(m_ofs.is_open()) {
        m_ofs << "---------------finish---------------\n";
        m_ofs.flush();
        m_ofs.close();
    }
#else
    if(util::isOpenedFD(m_filefd)) {
        const auto start_info = "---------------finish---------------\n";
        auto ret = WRITE(m_filefd, start_info, strlen(start_info) );
        FLUSH(m_filefd);
        CLOSE(m_filefd);
    }
#endif
}

// FIXME：并没有做到多个logger写同一个文件时的互斥
void FileLogAppender::WriteLog(const LogMessage::ptr& msg)
{
    assert(msg != nullptr);
// TICK_START()
    std::lock_guard lg{ILogAppender::m_mutex};
#if USE_CPP_STREAM
    m_ofs <<  m_formatter->format(msg);
    m_ofs.flush(); // 必须的，否则多线程写的情况下，在线程切换时会让日志混杂
#else
    // 保证写日志的原子性，使得日志按照生成的时间输出到文件
    const std::string & msg_str = m_formatter->format(msg);

    // 使用O_APPEND模式打开的文件的write()是原子操作：保证这一条信息写到内核缓冲队列中（缺点，来一条日志就写一条，频繁陷入内核态）
    auto ret = WRITE(m_filefd, msg_str.c_str(), msg_str.size());
#endif
// TICK_END_CALCAVG()
}

} // namespace yy::Ylog
