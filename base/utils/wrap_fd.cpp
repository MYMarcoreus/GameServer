#include "wrap_fd.h"
#include "log.h"
#include "status/Status.h"
#include <fcntl.h>
#include <cerrno>
#include <cstdio>

namespace yy::util {

#ifdef ____DEBUG
#define FATAL_ERROR_FD(cond, funname) \
if( cond ){ \
    YLOG_FATAL("GetFD<%d> "#funname"() error: %s", fd_, StatusCode{errno}.ToString().c_str()); \
    throw std::system_error{ errno, std::system_category() }; \
}
#else
#define FATAL_ERROR_FD(cond, funname) \
if( cond ){ \
    YLOG_FATAL("fd<%d> "#funname"() error: %s", fd_, StatusCode{errno}.ToString().c_str()); \
}
#endif


FileDescriper::FileDescriper(int fd): fd_{fd} {}

ssize_t FileDescriper::Read(void *buf, size_t nbytes) const
{
    ssize_t ret = ::read(fd_, buf, nbytes);
    if (ret < 0) {
        // ET模式，返回-1表示本次recv数据读取完毕
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return -1;
        } else {
            FATAL_ERROR_FD(true, read);
        }
    }
    return ret;
}

ssize_t FileDescriper::Write(const void *buf, size_t nbytes) const
{
    ssize_t ret = ::write(fd_, buf, nbytes);
    FATAL_ERROR_FD(ret < 0, write);
    return ret;
}

void FileDescriper::Close() const
{
    if(isValid()) {
        int ret = ::close(fd_);
        FATAL_ERROR_FD(ret < 0, close);
    }
}

int FileDescriper::Dup() const
{
    int ret = ::dup(fd_);
    FATAL_ERROR_FD(ret < 0, dup);
    return ret;
}

int FileDescriper::Dup2(const FileDescriper & newfd) const
{
    int ret = ::dup2(fd_, newfd.get_fd());
    FATAL_ERROR_FD(ret < 0, dup2);
    return ret;
}

void FileDescriper::SetNonblocking() const
{
    int old_option = ::fcntl(fd_, F_GETFL);
    ::fcntl(fd_, F_SETFL, old_option | O_NONBLOCK);
}

void FileDescriper::Flush() const
{
    int ret = fsync(fd_);
    FATAL_ERROR_FD(ret < 0, fsync);
}

bool FileDescriper::isValid() const
{
    return fd_ >= 3;
}

bool FileDescriper::isOpened() const
{
    return fcntl(fd_, F_GETFD) != -1;
}



}