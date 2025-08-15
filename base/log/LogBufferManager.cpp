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

    // 若m_current未满，则写入该缓冲区
    if (m_current->HaveEnoughFreeSpace(static_cast<int>(logstr.size())))
    {
        m_current->AppendDataFromArray(logstr);
    }
    // 若m_current已满，说明要纳入文件待写区
    else
    {
        // 纳入文件待写区
        m_buffersToWrite.emplace_back(std::move(m_current));

        // 使用 next 作为新的 current，如果 next 不存在则新建
        if (m_next) {
            m_current = std::move(m_next); //! 双缓冲区核心，直接切换到备用缓冲区，而不是阻塞等待后台线程将当前缓冲区写完
        } else {
            m_current = std::make_unique<LinearBuffer>(m_bufferSize);
        }

        m_current->AppendDataFromArray(logstr);
    }
}



void LogBufferManager::SwapAndWriteFlush(std::chrono::milliseconds flush_interval) {
    //! 交换
    {
        std::unique_lock lock(m_mutex);

        // m_current有数据，就将其加入文件待写区
        if (m_current) {
            m_buffersToWrite.emplace_back(std::move(m_current));
        }

        // 将待写到文件的数据安全地交换出去
        //! 重点：只能用swap，不能用下面两行
        tempBuffersToWrite_.swap(m_buffersToWrite);

        // 重新分配 current 和 next
        m_current = std::move(tempBuffer1_);
        if (m_next == nullptr) {
            m_next =  std::move(tempBuffer2_);
        }
    }

    //! Write
    if (writeCb_)
        writeCb_(tempBuffersToWrite_);

    // 修改缓冲区截断逻辑
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

    //! Flush
    if (m_flushCb)
        m_flushCb();

}


}
