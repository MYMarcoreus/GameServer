#ifndef ____WRAP_FDSET_H
#define ____WRAP_FDSET_H

#include "wrap_fd.h"
#include<sys/select.h>
#include<unistd.h>


namespace yy::util {

class FdSet
{
private:
    fd_set fdset_{};
public:
    FdSet() { this->clear(); }

    // 将fdset变量所有位归0
    void clear() { FD_ZERO(&fdset_); }

    // 将第fd号位置1：将文件描述符fd加入监视列表
    void add_fd(FileDescriper fd) { FD_SET(fd.get_fd(), &fdset_); }

    // 将第fd号位归0：将文件描述符fd的从监视列表中清除
    void del_fd(FileDescriper fd) { FD_CLR(fd.get_fd(), &fdset_); }

    // 查询第fd号位：查询文件描述符fd是否在监视列表中
    bool has_fd(FileDescriper fd) { return FD_ISSET(fd.get_fd(), &fdset_); }

    fd_set &get_fdset() { return fdset_; }
};

} // namespace yy


#endif // !____WRAP_FDSET_H
