#include "LogBufferManager.h"
#include "ILogAppender.h"
#include "log.h"
#include "LinearBuffer.h"

using yy::util::LinearBuffer;

namespace yy::Ylog
{
class ILogAppender;

LogBufferManager::LogBufferManager(int bufferSize, WriteCallback write_cb, FlushCallback flush_cb) :
    m_bufferSize(bufferSize),
    m_current(std::make_unique<LinearBuffer>(bufferSize)),
    m_next(std::make_unique<LinearBuffer>(bufferSize)),
    tempBuffer1_(std::make_unique<LinearBuffer>(bufferSize)),
    tempBuffer2_(std::make_unique<LinearBuffer>(bufferSize)),
    writeCb_(std::move(write_cb)),
    m_flushCb(std::move(flush_cb))
{
    m_buffersToWrite.reserve(16);
    tempBuffersToWrite_.reserve(16);
}

LogBufferManager::~LogBufferManager()
{
}

void LogBufferManager::Append(const std::string & logstr) {
    std::unique_lock lock(m_mutex);

    // 若主缓冲区没有足够的空间
    if (not m_current->HaveEnoughFreeSpace(static_cast<int>(logstr.size())))
    {
        // ① 将已满的主缓冲区纳入文件待写区
        m_buffersToWrite.emplace_back(std::move(m_current));
        //* ② 将「备用缓冲区」作为新的「主缓冲区」：双缓冲区核心，直接切换到备用缓冲区，而不是阻塞等待后台线程将当前缓冲区写完
        m_current = m_next ? std::move(m_next) : std::make_unique<LinearBuffer>(m_bufferSize);
    }

    // ③ 将日志写入主缓冲区
    m_current->AppendDataFromArray(logstr);
}



void LogBufferManager::SwapAndWriteFlush(std::chrono::milliseconds flush_interval) {
    //! Step1：交换
    {
        std::unique_lock lock(m_mutex);

        // 将主缓冲区加入文件待写区
        if (m_current) {
            m_buffersToWrite.emplace_back(std::move(m_current));
        }

        // 将「文件待写区的缓冲区」安全地交换到临时变量中：减少临界区的持续时间，使得 Write 部分是无锁的
        tempBuffersToWrite_.swap(m_buffersToWrite);

        // 重新分配 主/备用 缓冲区
        m_current = std::move(tempBuffer1_);
        if (m_next == nullptr) {
            m_next =  std::move(tempBuffer2_);
        }
    }

    //! Step2：无锁写文件
    if (writeCb_)
        writeCb_(tempBuffersToWrite_);

    //! Step3：回收Buffer
    if (tempBuffersToWrite_.size() > 2) {
        tempBuffersToWrite_.resize(2);
    }
    if (tempBuffer1_ == nullptr)
    {
        tempBuffer1_ = std::move(tempBuffersToWrite_.back());
        tempBuffersToWrite_.pop_back();
        tempBuffer1_->Reset();
    }
    if (tempBuffer2_ == nullptr)
    {
        tempBuffer2_ = std::move(tempBuffersToWrite_.back());
        tempBuffersToWrite_.pop_back();
        tempBuffer2_->Reset();
    }
    tempBuffersToWrite_.clear();

    //! Step4：Flush
    if (m_flushCb)
        m_flushCb();
}


}
