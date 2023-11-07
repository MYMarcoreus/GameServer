#include "FileDescriptor.h"
#include "log.h"
#include "status/Status.h"

#include <fcntl.h>
#include <cerrno>
#include <cstdio>

namespace yy {
namespace net {


FileDescriptor::FileDescriptor(int fd): fd_{fd} {}

ssize_t FileDescriptor::Read(void *buf, size_t nbytes) const
{
    ssize_t ret = ::read(fd_, buf, nbytes);
    if (ret < 0) {
        // ET模式，返回-1表示本次recv数据读取完毕
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return -1;
        } else {
            YLOG_FATAL("In FileDescriptor::Read(), ::read error %s", util::StatusCode(errno).ToString().c_str())
        }
    }
    return ret;
}

ssize_t FileDescriptor::Write(const void *buf, size_t nbytes) const
{
    ssize_t ret = ::write(fd_, buf, nbytes);
    if(ret < 0) {
        YLOG_FATAL("In FileDescriptor::Write(), ::write error %s", util::StatusCode(errno).ToString().c_str())
    }
    return ret;
}

void FileDescriptor::Close() const
{
    if(IsOpened()) {
        int ret = ::close(fd_);
        if(ret < 0) {
            YLOG_FATAL("In FileDescriptor::Close(), ::close error %s", util::StatusCode(errno).ToString().c_str())
        }
    }
}

int FileDescriptor::Dup() const
{
    int ret = ::dup(fd_);
    if(ret < 0) {
        YLOG_FATAL("In FileDescriptor::Dup(), ::dup error %s", util::StatusCode(errno).ToString().c_str())
    }
    return ret;
}

int FileDescriptor::Dup2(const FileDescriptor & newfd) const
{
    int ret = ::dup2(fd_, newfd.GetFD());
    if(ret < 0) {
        YLOG_FATAL("In FileDescriptor::Dup2(), ::dup2 error %s", util::StatusCode(errno).ToString().c_str())
    }
    return ret;
}

void FileDescriptor::SetNonblocking() const
{
    int old_option = ::fcntl(fd_, F_GETFL);
    if(old_option < 0) {
        YLOG_FATAL("In FileDescriptor::SetNonblocking(), ::fcntl error %s", util::StatusCode(errno).ToString().c_str())
    }
    ::fcntl(fd_, F_SETFL, old_option | O_NONBLOCK);
}

void FileDescriptor::Flush() const
{
    int ret = ::fsync(fd_);
    if(ret < 0) {
        YLOG_FATAL("In FileDescriptor::Flush(), ::fsync error %s", util::StatusCode(errno).ToString().c_str())
    }
}

bool FileDescriptor::IsOpened() const
{
    return ::fcntl(fd_, F_GETFD) != -1;
}

FileDescriptor::~FileDescriptor() {
    int ret = ::close(fd_);
    if(ret < 0) {
        YLOG_FATAL("In FileDescriptor::~FileDescriptor(), ::close error %s", util::StatusCode(errno).ToString().c_str())
    }
}


} // yy
} // net