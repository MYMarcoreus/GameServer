#include "FullDuplexPipe.h"
#include<cstdio>
#include<unistd.h>
#include<sys/socket.h>


namespace yy::util
{

FullDuplexPipe::FullDuplexPipe(): pipefds_{ }
{
    int fds[2];

    // 使用socketpair实现两端都可读写的管道
    socketpair(PF_UNIX, SOCK_STREAM, 0, fds);
    // ::Pipe(fds); 而pipe系统调用实现的管道fds[1]只能write，fds[0]只能read

    pipefds_[0] = fds[0];
    pipefds_[1] = fds[1];
}

FullDuplexPipe::~FullDuplexPipe()
{
    close(pipefds_[0]);
    close(pipefds_[1]);
}

int FullDuplexPipe::Write(const void* __buf, size_t __n)
{
    const auto ret = write(sideW(), __buf, __n);
    return ret < 0 ? 0 : ret;
}

int FullDuplexPipe::Read(void* __buf, size_t __n)
{
    const auto ret = read(sideR(), __buf, __n);
    return ret < 0 ? 0 : ret;
}


}
