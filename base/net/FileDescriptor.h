#ifndef LINUXGAMESERVER_FILEDESCRIPTOR_H
#define LINUXGAMESERVER_FILEDESCRIPTOR_H

#include <unistd.h>

///@brief 文件描述符的RAII类
namespace yy::net {

class FileDescriptor {
public:
    // 允许隐式int->FileDescriper
    explicit FileDescriptor(int fd);

    explicit operator int() const { return fd_;  } // NOLINT(google-explicit-constructor)

    // 自动Close()
    ~FileDescriptor();


    // 获取文件描述符
    int GetFD() const { return fd_; }

    // Recv：只有ET模式下才会返回-1以表示数据读取完毕
    ssize_t Read(void *buf, size_t nbytes) const;

    ssize_t Write(const void *buf, size_t nbytes) const;

    void Close() const;

    ///@brief 复制文件描述符，由内核分配新的文件描述符
    int Dup() const;

    ///@brief 复制文件描述符，调用者指定新描述符newfd，如果该newfd已存在则将其关闭，然后再分配
    int Dup2(const FileDescriptor & newfd) const;

    ///@brief 设置文件描述符为非阻塞I/O
    void SetNonblocking() const;

    ///@brief 将write后的仍在缓冲区内的数据flush到磁盘上
    void Flush() const;

    ///@brief 判断该文件描述符是否打开
    bool IsOpened() const;

public:
    static const int kInvalidFD = ~0;

protected:
    int fd_;
};

} // yy::net

#endif //LINUXGAMESERVER_FILEDESCRIPTOR_H
