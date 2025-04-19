#include "FileLogAppender.h"

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
    m_filefd = ::open(m_logfilepath.c_str(), O_CREAT | O_APPEND | O_WRONLY, 0644);
    assert(m_filefd != -1);

    auto start_info = "---------------start---------------\n";
    auto ret  = ::write(m_filefd, start_info, strlen(start_info) );
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
        auto start_info = "---------------finish---------------\n";
        auto ret = ::write(m_filefd, start_info, strlen(start_info) );

        FLUSH(m_filefd);
        ::close(m_filefd);
        // assert( ::fclose(m_filep) != EOF);
    }
#endif
}

// FIXME：并没有做到多个logger写同一个文件时的互斥
void FileLogAppender::WriteLog(const LogMessage::ptr& msg)
{
    assert(msg != nullptr);
// TICK_START()
#if USE_CPP_STREAM
    std::lock_guard lg{ILogAppender::m_mutex};
    m_ofs <<  m_formatter->format(msg);
    m_ofs.flush(); // 必须的，否则多线程写的情况下，在线程切换时会让日志混杂
#else
    // 保证写日志的原子性，使得日志按照生成的时间输出到文件
    std::lock_guard lg{m_mutex};
    const std::string & msg_str = m_formatter->format(msg);

    // 使用O_APPEND模式打开的文件的write()是原子操作：保证这一条信息写到内核缓冲队列中
    auto ret = ::write(m_filefd, msg_str.c_str(), msg_str.size());
    assert(ret != -1);
    // assert(::fsync(m_filefd) != -1); //! bug所在，使得写入的数量减少了!!!
#endif
// TICK_END_CALCAVG()
}

} // namespace yy::Ylog
