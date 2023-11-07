#ifndef ____WRAP_SOCKET_H
#define ____WRAP_SOCKET_H

#include "wrap_fd.h"
#include "IPAddress.h"
#include "noncopyable.h"
#include "log.h"
#include "status/Status.h"

#include <sys/socket.h>
#include <arpa/inet.h>


namespace yy::util {

using ::yy::net::IPAddress;


class File;


enum : bool
{
    OFF = false,
    ON = true
};


enum class SocketType
{
    TCP = SOCK_STREAM,
    UDP = SOCK_DGRAM
};

//! 本对象只是一个方法类，而不是一个管理类，所以允许复制，所以不允许也不会在析构时关闭套接字
class Socket : public FileDescriper
{
public:
    // 接管现有套接字
    Socket(FileDescriper fd);

    // 创建并管理套接字
    explicit Socket(SocketType type);


    void Init(SocketType type);

    ~Socket() = default;

/* 服务器 */
    // 绑定本端地址
    void Bind(const sockaddr_in *local_addr);

    // 绑定本端地址：直接传入本端IP地址和端口
    void Bind(const char *ip, uint16_t port);

    // 绑定本端端口：系统分配IP地址
    void Bind(uint16_t port);

    // 绑定本端地址
    void Bind(IPAddress::ptr local_addr);

    // 将本端套接字设置为监听套接字
    void Listen(int backlog = SOMAXCONN);

    // 等待客户连接，并返回传输套接字
    Socket Accept(struct sockaddr_in *sa = nullptr);


/* 客户端 */
    // 绑定对端地址
    void Connect(const struct sockaddr_in *sa);

    // 绑定对端地址：直接传入对端IP地址和端口
    void Connect(const char *ip, uint16_t port);

    void Connect(IPAddress::ptr peer_addr);


/* TCP的I / O函数 */
    // Recv：只有ET模式下才会返回-1以表示数据读取完毕
    ssize_t Recv(void *ptr, size_t nbytes, int flags = 0);

    ssize_t Send(const void *ptr, size_t nbytes, int flags = 0);

/* UDP的I / O函数 */
    ssize_t Sendto(const void *ptr, size_t nbytes, int flags, const struct sockaddr_in *sa);

    ssize_t Sendto(const void *ptr, size_t nbytes, int flags, const char *ip, uint16_t port);

    ssize_t Sendto(const void *ptr, size_t nbytes, int flags, IPAddress::ptr peer_addr);

    // Recvfrom：只有ET模式下才会返回-1以表示数据读取完毕
    ssize_t Recvfrom(void *ptr, size_t nbytes, int flags, struct sockaddr_in *sa);

    // Recvfrom：只有ET模式下才会返回-1以表示数据读取完毕
    ssize_t Recvfrom(void *ptr, size_t nbytes, int flags, const char *ip, uint16_t port);

    // Recvfrom：只有ET模式下才会返回-1以表示数据读取完毕
    ssize_t Recvfrom(void *ptr, size_t nbytes, int flags, IPAddress::ptr peer_addr);


    // 发送一个文件
    ssize_t Sendfile(const File &file, off_t *offset, size_t count);

/*套接字选项 */
    // 设置套接字选项
    template<typename T>
    void SetOpt(int SO_XXXX, const T &optval)
    {
        int ret = ::setsockopt(fd_, SOL_SOCKET, SO_XXXX, &optval, sizeof(optval));
        if( ret < 0 ){
            YLOG_FATAL("socket<%d> setsockopt() error: %s", fd_, StatusCode{errno}.ToString().c_str())
        }
    }

    void SetOpt_reuseaddr(bool onoff);

    void SetOpt_linger(bool onoff, int timeout = 0);

    // 获取套接字选项
    void Getopt(int level, int SO_XXXX, void *optval, socklen_t *optlenptr);

/*套接字连接两端的地址 */
    // 获取本端地址
    [[nodiscard]] IPAddress::ptr GetSockname() const ;

    // 获取对端地址
    [[nodiscard]] IPAddress::ptr GetPeername() const;

    // 绑定本端地址
    void SetSockname(const char *ip, uint16_t port) { this->Bind(ip, port); }

    // 绑定对端地址
    void SetPeername(const char *ip, uint16_t port) { this->Connect(ip, port); }


/* 关闭套接字 */
    // 直接关闭套接字连接(不管套接字的引用计数)
    void Shutdown(int how = SHUT_RDWR);

    // 断开输入流(读端)
    void ShutdownRD() { Shutdown(SHUT_RD); }

    // 断开输出流(写端)，使得套接字处于半关闭的状态
    void Shutdown_WR() { Shutdown(SHUT_WR); }

    inline bool operator==(int fd) const { return fd_ == fd; }

    inline bool operator==(Socket obj) const { return fd_ == obj.fd_; }

    inline bool operator!=(int fd) const { return fd_ != fd; }

    inline bool operator!=(Socket obj) const { return fd_ != obj.fd_; }

private:
    static sockaddr_in ipport2sockaddr_in(const char *ip, uint16_t port);

    static IPAddress::ptr ipport2ipaddr(const char *ip, uint16_t port);
};


inline bool operator==(int fd, Socket obj) { return obj == fd; }

inline bool operator!=(int fd, Socket obj) { return obj != fd; }


// 定义仿函数使得UserID和Socket可以作为map的key
struct hash_fn
{
    size_t operator()(util::Socket obj) const
    {
        return std::hash<int32_t>()(obj.get_fd());
    }
};



}


#endif // !____WRAP_SOCKET_H
