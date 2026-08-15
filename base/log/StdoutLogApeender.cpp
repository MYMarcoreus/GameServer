#include "StdoutLogApeender.h"
#include "LinearBuffer.h"
#include "LogBufferManager.h"
#include "LogFormatter.h"
#include <iostream>

namespace yy::Ylog {

/******************************* StdoutLogApeender *******************************/
StdoutLogApeender::StdoutLogApeender(const std::string &format_pattern, const int buffer_size, const std::chrono::milliseconds flush_interval):
    ILogAppender(format_pattern, buffer_size, flush_interval)
{}

StdoutLogApeender::~StdoutLogApeender()
{
}


void StdoutLogApeender::WriteLog(const std::shared_ptr<LogMessage> & msg) {
    std::lock_guard lg{ILogAppender::m_mutex};
    std::cout << m_formatter->format(msg);
    std::cout.flush();
}


void StdoutLogApeender::Write(const BufferVector& outBuffersToWrite)
{
    if (outBuffersToWrite.size() > 0 and outBuffersToWrite[0]->GetDataSize() != 0) {
        for (const auto& buffer : outBuffersToWrite) {
            std::cout << buffer->PopAllDataAsString();
        }
    }
}

void StdoutLogApeender::Flush()
{
    std::cout.flush();
}
}
