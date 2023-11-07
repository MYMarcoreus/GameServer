#ifndef ____WRAP_PIPE_H
#define ____WRAP_PIPE_H


#include"wrap_fd.h"
#include<cstdio>
#include<unistd.h>
#include<sys/socket.h>


namespace yy::util {

class FullDuplexPipe
{
public:
    FullDuplexPipe(): pipefds_{ FileDescriper::kInvalidFD, FileDescriper::kInvalidFD}
    {
        int fds[2];

        // 使用socketpair实现两端都可读写的管道
        ::socketpair(PF_UNIX, SOCK_STREAM, 0, fds);
        // ::Pipe(fds); 而pipe系统调用实现的管道fds[1]只能write，fds[0]只能read

        pipefds_[0].fd_ = fds[0];
        pipefds_[1].fd_ = fds[1];
    }

    // 一般作写端write(sideW)
    [[nodiscard]] FileDescriper sideW()  { return pipefds_[1];}

    // 一般作读端read(sideR)
    [[nodiscard]] FileDescriper sideR()  { return pipefds_[0];}

private:
    FileDescriper pipefds_[2];
};


}






#endif //____WRAP_PIPE_H
