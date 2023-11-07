#ifndef ____WRAP_FD_H
#define ____WRAP_FD_H

#include <unistd.h>

namespace yy::util {

// 其实FileDescriper只是一个行为类，用于定义文件描述符的行为，不管理文件描述符的生命周期，可以尽情复制。
// 通过让File和Socket继承FileDescriper，让他们继承了FileDescriper的行为。
// 因为这样的继承并不会使用到多态性，故不定义虚函数，从而避免虚函数指针的额外开销(16Byte→4Byte)。
class FileDescriper
{
    friend class FullDuplexPipe;
public:
    // 允许隐式int->FileDescriper
    FileDescriper(int fd); // NOLINT(google-explicit-constructor)

    // 允许隐式FileDescriper->int
    operator int() const { return fd_;  } // NOLINT(google-explicit-constructor)

    // 由子类实现自动Close()
    ~FileDescriper() = default;


    // 获取文件描述符
    [[nodiscard]] int get_fd() const { return fd_; }

    // Recvfrom：只有ET模式下才会返回-1以表示数据读取完毕
    ssize_t Read(void *buf, size_t nbytes) const;

    ssize_t Write(const void *buf, size_t nbytes) const;

    void Close() const;

    // 复制文件描述符，由内核分配新的文件描述符
    [[nodiscard]] int Dup() const;

    // 复制文件描述符，调用者指定新描述符newfd，如果该newfd已存在则将其关闭，然后再分配
    [[nodiscard]] int Dup2(const FileDescriper & newfd) const;

    // 设置套接字为非阻塞I/O
    void SetNonblocking() const;

    // 将write后的仍在缓冲区内的数据flush到磁盘上
    void Flush() const;

    [[nodiscard]] bool isValid() const;

    [[nodiscard]] bool isOpened() const;

public:
    static const int kInvalidFD = ~0;

protected:
    int fd_;
};

inline bool operator==(const FileDescriper & lhs, const FileDescriper & rhs) { return lhs.get_fd() == rhs.get_fd(); }
inline bool operator==(const FileDescriper & lhs, int rhs) { return lhs.get_fd() == rhs; }
inline bool operator==(int lhs, const FileDescriper & rhs) { return lhs == rhs.get_fd(); }


} // namespace yy




#endif // !____WRAP_FD_H
