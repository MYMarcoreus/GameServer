#include "StdoutLogApeender.h"

namespace yy::Ylog {

/******************************* StdoutLogApeender *******************************/
void StdoutLogApeender::WriteLog(const LogMessage::ptr& msg) {
    std::lock_guard lg{ILogAppender::m_mutex};
    std::cout << m_formatter->format(msg);
    std::cout.flush();
}

StdoutLogApeender::StdoutLogApeender(const std::string &format_pattern)
    : m_formatter(std::make_shared<LogFormatter>(format_pattern)) {}

}