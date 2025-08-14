#pragma once

#ifdef ____LINUX

#include <functional>
#include <memory>
#include <signal.h>
#include "FullDuplexPipe.h"


namespace yy::net
{
class EventLoop;
class IOChannel;
}

namespace yy::net
{


class SignalManager {
public:
    SignalManager(EventLoop * loop, const std::function<void()>& handler);
    ~SignalManager();

    static auto WritePipe(int sig) -> void;
    static auto ReadPipe() -> std::string;

    static util::FullDuplexPipe pipe_;

    /// @brief 设置信号SIGXXXX的处理函数为sighandler，
    static void set_signal_handler(int SIGXXXX, sighandler_t sighandler);
    ///@brief 忽略信号
    static void set_signal_ignore(int SIGXXXX);
    ///@brief 将信号的处理函数恢复为系统默认
    static void set_signal_handler_default(int SIGXXXX);

private:
    EventLoop * loop_;
    std::unique_ptr<IOChannel> channel_;
};

}
#endif
