#ifndef GAMESERVER_FULLDUPLEXPIPE_H
#define GAMESERVER_FULLDUPLEXPIPE_H

#ifdef  ____LINUX

#include<cstdio>
#include<unistd.h>
#include<sys/socket.h>
#include<type_traits>


namespace yy::util {

class FullDuplexPipe
{
public:
    FullDuplexPipe(): pipefds_{ }
    {
        int fds[2];

        // 使用socketpair实现两端都可读写的管道
        ::socketpair(PF_UNIX, SOCK_STREAM, 0, fds);
        // ::Pipe(fds); 而pipe系统调用实现的管道fds[1]只能write，fds[0]只能read

        pipefds_[0] = fds[0];
        pipefds_[1] = fds[1];
    }

    ~FullDuplexPipe() {
        ::close(pipefds_[0]);
        ::close(pipefds_[1]);
    }

    int Write(const void * __buf, size_t __n) {
        auto ret = ::write(sideW(), __buf, __n);
        return ret < 0 ? 0 : ret;
    }
    template<class T>
    requires requires {
        !std::is_pointer_v<T>;
    }
    int Write(const T & val) {
        auto ret = ::write(sideW(), &val, sizeof(T));
        return ret < 0 ? 0 : ret;
    }

    int Read(void * __buf, size_t __n) {
        auto ret = ::read(sideR(), __buf, __n);
        return ret < 0 ? 0 : ret;
    }
    template<class T>
    requires requires {
        !std::is_pointer_v<T>;
    }
    int Read(T & val) {
        auto ret = ::read(sideR(), &val, sizeof(T));
        return ret < 0 ? 0 : ret;
    }


    // 一般作写端write(sideW)
    int sideW()  { return pipefds_[1];}

    // 一般作读端read(sideR)
    int sideR()  { return pipefds_[0];}

private:
    int pipefds_[2];
};


}



#endif

#endif //GAMESERVER_FULLDUPLEXPIPE_H

