#include "SignalManager.h"
#include "IOChannel.h"
#include "util_functions.h"

namespace yy::util
{

SignalManager::SignalManager(EventLoop* loop, const std::function<void()>& handler):
    loop_{loop},
    channel_{std::make_unique<IOChannel>(loop_, pipe_.sideR(), "Wakeup Eventfd Channel")}
{
    // 屏蔽SIGPIPE：当服务器进程向已收到RST的用户套接字执行写操作时，内核会向进程发送SIGPIPE信号来结束进程
    set_signal_ignore(SIGPIPE);

    channel_->SetReadCallback(handler);
    channel_->EnableReading();

    // 设置三个信号处理函数：该处理函数将信号通过管道传送
    set_signal_handler(SIGALRM, WritePipe);
    set_signal_handler(SIGINT, WritePipe);/* Ctrl+c */
    set_signal_handler(SIGTERM, WritePipe);// kill <pid>
}

void SignalManager::WritePipe(int sig)
{
    //! 向唤醒事件文件描述符进行写，以触发其eoll事件
    const int msg = sig;
    pipe_.Write((const char *)&msg, 1);
}

std::string SignalManager::ReadPipe()
{
    //! 向唤醒事件文件描述符进行写，以触发其epoll事件
    static std::string sigs;
    sigs.assign(128, 0);
    auto nSig = pipe_.Read(sigs.data(), sigs.size());
    return sigs;
}

void SignalManager::set_signal_handler(const int SIGXXXX, const sighandler_t sighandler)
{
    ::signal(SIGXXXX, sighandler);
}

void SignalManager::set_signal_ignore(const int SIGXXXX)
{
    set_signal_handler(SIGXXXX, SIG_IGN);
}

void SignalManager::set_signal_handler_default(const int SIGXXXX)
{
    set_signal_handler(SIGXXXX, SIG_DFL);
}

FullDuplexPipe SignalManager::pipe_{};

}