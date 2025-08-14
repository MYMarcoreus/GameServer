#ifdef ____WINDOWS
#include "IOCPPoller.h"
#include "log.h"



namespace yy::net
{
IOCPPoller::IOCPPoller(EventLoop* loop)
    : Poller(loop)
{
    m_iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, -1);
    if (m_iocpHandle == nullptr) {
        fprintf(stderr,"epoll_create1() error");
    }
}

IOCPPoller::~IOCPPoller()
{
    ::CloseHandle(m_iocpHandle);

}

void IOCPPoller::PollWait(ChannelList& activeChannel, std::chrono::milliseconds timeout)
{
    DWORD bytesTransferred = 0;
    ULONG_PTR complete_key = 0;
    LPOVERLAPPED overlapped  = nullptr;
    const DWORD dwMilliseconds = static_cast<DWORD>(timeout.count());

    auto bRetValue = GetQueuedCompletionStatus(m_iocpHandle, &bytesTransferred, &complete_key, &overlapped , dwMilliseconds);
    if(!bRetValue)
    {
        if (overlapped  == nullptr)
        {
            if (GetLastError() == WAIT_TIMEOUT) {
                // 正常的超时退出
            } else {
                // 错误处理（日志、退出等）
                YLOG_ERROR("GetQueuedCompletionStatus() error");
            }
        } else {
            // 否则，是异步I/O失败，仍然有 overlapped → 可以处理失败事件
        }
    }

    // overlapped 不为空，说明有 I/O 完成事件
    // 通常我们用 OVERLAPPED 结构体嵌套自定义的数据（Context）
    // IOContext* context = CONTAINING_RECORD(overlapped, IOContext, overlapped);
}
}


#endif