#include "StdoutLogApeender.h"

#include <cassert>

#include "cross_platform_defines.h"
#include <iostream>

#include "LinearBuffer.h"

namespace yy::Ylog {

/******************************* StdoutLogApeender *******************************/
StdoutLogApeender::StdoutLogApeender(const std::string &format_pattern, const int buffer_size, const std::chrono::milliseconds flush_interval):
    ILogAppender(format_pattern, buffer_size, flush_interval),
    m_newBuffer1(std::make_unique<util::LinearBuffer>(m_BuffferSize)),
    m_newBuffer2(std::make_unique<util::LinearBuffer>(m_BuffferSize)),
    m_buffersToWrite{}
{}

StdoutLogApeender::~StdoutLogApeender()
{
}


void StdoutLogApeender::WriteLog(const std::shared_ptr<LogMessage> & msg) {
    std::lock_guard lg{ILogAppender::m_mutex};
    std::cout << m_formatter.format(msg);
    std::cout.flush();
}

void StdoutLogApeender::AppendBuffer(const std::shared_ptr<LogMessage>& msg)
{
    m_bufferManager.Append(m_formatter.format(msg));
}

void StdoutLogApeender::FlushBuffer()
{
    m_bufferManager.Swap(m_newBuffer1, m_newBuffer2, m_buffersToWrite, m_FlushInterval);

    if (m_buffersToWrite.size() > 0 and m_buffersToWrite[0]->GetDataSize() != 0) {
        for (const auto& buffer : m_buffersToWrite) {
            std::cout << buffer->PopAllDataAsString();
        }
    }

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
    std::cout.flush();
}
}
