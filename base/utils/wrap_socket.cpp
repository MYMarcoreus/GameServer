#include "wrap_socket.h"
#include "wrap_file.h"
#include <sys/sendfile.h>
#include <memory>
#include<stdexcept>
#include <cstring>

namespace yy::util {

using net::IPv4Address;

#ifdef ____DEBUG
#define FATAL_ERROR_SOCKET_SYS(cond, funname) \
if( cond ){ \
    YLOG_FATAL("socket<%d> "#funname"() error: %s", fd_, StatusCode{errno}.ToString().c_str()) \
    throw std::system_error{ errno, std::system_category() }; \
}
#else
#define FATAL_ERROR_SOCKET_SYS(cond, funname) \
if( cond ){ \
    YLOG_FATAL("socket<%d> "#funname"() error: %s", fd_, StatusCode{errno}.ToString().c_str()) \
}
#endif


Socket::Socket(FileDescriper fd): FileDescriper(fd) {}

Socket::Socket(SocketType type): FileDescriper(kInvalidFD)
{
    this->Init(type);
}

void Socket::Init(SocketType type)
{
    if (type == SocketType::TCP) {
        fd_ = ::socket(PF_INET, SOCK_STREAM, 0);
    } else if (type == SocketType::UDP) {
        fd_ = ::socket(PF_INET, SOCK_DGRAM, 0);
    } else {
        throw std::invalid_argument("wrong socket type, only TCP or UDP");
    }
}

Socket Socket::Accept(sockaddr_in *sa)
{
    socklen_t salen = sizeof(sockaddr_in);

    int fd = ::accept(fd_, (struct sockaddr *) sa, &salen);
    FATAL_ERROR_SOCKET_SYS(fd < 0, accept);

    return Socket(fd);
}

void Socket::Bind(const sockaddr_in *local_addr)
{
    int ret = ::bind(fd_, (sockaddr *) local_addr, sizeof(sockaddr_in));
    FATAL_ERROR_SOCKET_SYS(ret < 0, bind)
    //sockaddr_ = *sa;
}

void Socket::Bind(const char *ip, uint16_t port)
{
    auto &&local_addr = ipport2sockaddr_in(ip, port);
    this->Bind(&local_addr);
}

void Socket::Bind(IPAddress::ptr local_addr)
{
    this->Bind((sockaddr_in *) local_addr->GetRawAddr());
}

void Socket::Bind(uint16_t port)
{
    this->Bind(std::make_shared<IPv4Address>(port));
}

void Socket::Connect(const sockaddr_in *peer_addr)
{
    int ret = ::connect(fd_, (struct sockaddr *) peer_addr, sizeof(sockaddr_in));
    FATAL_ERROR_SOCKET_SYS(ret < 0, connect);
    //peeraddr_ = *sa;
}

void Socket::Connect(const char *ip, uint16_t port)
{
    auto &&peer_addr = ipport2sockaddr_in(ip, port);
    this->Connect(&peer_addr);
}

void Socket::Connect(IPAddress::ptr peer_addr)
{
    this->Connect((sockaddr_in *) peer_addr->GetRawAddr());
}

void Socket::Listen(int backlog)
{
    int ret = ::listen(fd_, backlog);
    FATAL_ERROR_SOCKET_SYS(ret < 0, listen);
}

ssize_t Socket::Recv(void *ptr, size_t nbytes, int flags)
{
    ssize_t ret = ::recv(fd_, ptr, nbytes, flags);
    if (ret < 0) {
        // ET模式，返回-1表示本次recv数据读取完毕
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return -1;
        }
        else if(errno == ECONNRESET) {
            return -2;
        }
        else if(errno == EBADF) {
            return -3;
        }
        else {
            FATAL_ERROR_SOCKET_SYS(true, recv);
        }
    }
    return ret;
}

ssize_t Socket::Send(const void *ptr, size_t nbytes, int flags)
{
    ssize_t ret = ::send(fd_, ptr, nbytes, flags);
    FATAL_ERROR_SOCKET_SYS(ret < 0, send);
    return ret;
}

ssize_t Socket::Sendto(const void *ptr, size_t nbytes, int flags, const sockaddr_in *sa)
{
    ssize_t ret = ::sendto(fd_, ptr, nbytes, flags, (struct sockaddr *) sa, sizeof(sockaddr_in));
    FATAL_ERROR_SOCKET_SYS(ret < 0, sendto);
    //peeraddr_ = *sa;
    return ret;
}

ssize_t Socket::Sendto(const void *ptr, size_t nbytes, int flags, const char *ip, uint16_t port)
{
    struct sockaddr_in sa = ipport2sockaddr_in(ip, port);
    return this->Sendto(ptr, nbytes, flags, &sa);
}

ssize_t Socket::Sendto(const void *ptr, size_t nbytes, int flags, IPAddress::ptr peer_addr)
{
    return this->Sendto(ptr, nbytes, flags, (sockaddr_in *) peer_addr->GetRawAddr());
}

ssize_t Socket::Recvfrom(void *ptr, size_t nbytes, int flags, sockaddr_in *sa)
{
    socklen_t salen = sizeof(sockaddr_in);
    ssize_t ret = ::recvfrom(fd_, ptr, nbytes, flags, (struct sockaddr *) sa, &salen);
    if (ret < 0) {
        // ET模式，返回-1表示本次recv数据读取完毕
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return -1;
        } else {
            FATAL_ERROR_SOCKET_SYS(ret < 0, recvfrom);
        }
    }
    //peeraddr_ = *sa;
    return ret;
}

ssize_t Socket::Recvfrom(void *ptr, size_t nbytes, int flags, const char *ip, uint16_t port)
{
    struct sockaddr_in sa = ipport2sockaddr_in(ip, port);
    return this->Recvfrom(ptr, nbytes, flags, &sa);
}

ssize_t Socket::Recvfrom(void *ptr, size_t nbytes, int flags, IPAddress::ptr peer_addr)
{
    return this->Recvfrom(ptr, nbytes, flags, (sockaddr_in *) peer_addr->GetRawAddr());
}


ssize_t Socket::Sendfile(const File &file, off_t *offset, size_t count)
{
    ssize_t ret = ::sendfile(file.get_fd(), fd_, offset, count);
    FATAL_ERROR_SOCKET_SYS(ret < 0, recvfrom);
    return ret;
}




void Socket::SetOpt_reuseaddr(bool onoff)
{
    int opt_val = onoff;
    SetOpt(SO_REUSEADDR, opt_val);
}

void Socket::SetOpt_linger(bool onoff, int timeout)
{
    struct linger ling{};
    ling.l_onoff = onoff;
    ling.l_linger = timeout;
    SetOpt(SO_LINGER, ling);
}

void Socket::Getopt(int level, int optname, void *optval, socklen_t *optlenptr)
{
    int ret = ::getsockopt(fd_, level, optname, optval, optlenptr);
    FATAL_ERROR_SOCKET_SYS(ret < 0, getsockopt);
}

IPAddress::ptr Socket::GetSockname() const
{
    sockaddr local_addr{};
    socklen_t salen = sizeof(sockaddr_in);
    int ret = ::getsockname(fd_, &local_addr, &salen);
    return ret<0 ? nullptr : std::make_shared<IPv4Address>((sockaddr_in *) &local_addr);
}

IPAddress::ptr Socket::GetPeername() const
{
    sockaddr local_addr{};
    socklen_t salen = sizeof(sockaddr_in);
    int ret = ::getpeername(fd_, &local_addr, &salen);
    FATAL_ERROR_SOCKET_SYS(ret < 0, getsockname);

    if(ret < 0)
        return nullptr;
    else {
        // if(salen == sizeof(sockaddr_in))
            return std::make_shared<IPv4Address>((sockaddr_in*)&local_addr);
        // else
            // return std::make_shared<class IPv6Address>((sockaddr_in6*)&local_addr);
    }
}

void Socket::Shutdown(int how)
{
    int ret = ::shutdown(fd_, how);
    if(ret < 0) {
        // 用户连接reset后，再调用shutdown，则会造成该情况
        if(errno == ENOTCONN)
            return;
    }
    FATAL_ERROR_SOCKET_SYS(ret < 0, shutdown);
}

sockaddr_in Socket::ipport2sockaddr_in(const char *ip, uint16_t port)
{
    struct sockaddr_in sa{};
    bzero(&sa, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    inet_pton(AF_INET, ip, &sa.sin_addr);
    return sa;
}

IPAddress::ptr Socket::ipport2ipaddr(const char *ip, uint16_t port)
{
    return std::make_shared<IPv4Address>(ip, port);
}



}