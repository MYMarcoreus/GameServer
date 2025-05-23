#include "SocketApiWrapper.h"
#include "IPAddress.h"
#include "log.h"
#include "status/Status.h"
#include "ErrnoSaver.h"
#include <fcntl.h>
#include "util_functions.h"



#include "net_definations.h"


namespace yy::SocketApiWrapper {

int64_t get_last_socket_error() {
#ifdef ____WINDOWS
    return GetLastError();
#elif defined(____LINUX)
    return errno;
#else
    #error Platform not supported
#endif
}



socket_t create_or_die(sa_family_t family, __socket_type type, bool isNonblock) {
#ifdef ____LINUX
    int realType = isNonblock ? (int) type | SOCK_NONBLOCK | SOCK_CLOEXEC : (int) type;
    socket_t sockfd = ::socket(family, realType, 0);
    if (sockfd < 0) {
        YLOG_FATAL("In Socket::Socket(), socket() error: {}", yy::util::GetLastErrorInfo());
    }
    return sockfd;
#elif defined(____WINDOWS)
    static std::once_flag wsa_init_flag;
    std::call_once(wsa_init_flag, [] {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            YLOG_FATAL("WSAStartup failed");
        }
    });

    socket_t sockfd = socket(family, type, 0);
    if (sockfd == INVALID_SOCKET) {
        YLOG_FATAL("In Socket::Socket(), socket() error: {}", yy::util::GetLastErrorInfo());
    } else if (isNonblock) {
        set_nonblocking(sockfd);
    }
    return sockfd;
#else
    #error Platform not supported
#endif

}

socket_t create_tcp_or_die(bool isNonblock) {
    return create_or_die(AF_INET, SOCK_STREAM, isNonblock);
}

socket_t create_udp_or_die(bool isNonblock) {
    return create_or_die(AF_INET, SOCK_DGRAM, isNonblock);
}



void listen_or_die(socket_t sockfd, int backlog) {
    int ret = ::listen(sockfd, backlog);
#ifdef ____WINDOWS
    if (ret == SOCKET_ERROR) {
#endif
#ifdef ____LINUX
        if (ret < 0) {
#endif
        SocketApiWrapper::close(sockfd);
        YLOG_FATAL("In Socket::StartListen(), listen() error: {}", yy::util::GetLastErrorInfo())
    }
}

void bind_or_die(socket_t sockfd, const std::shared_ptr<IPAddress> &localAddr) {
    int ret = ::bind(sockfd, localAddr->GetRawAddr(), localAddr->GetRawAddrLen());
#ifdef ____WINDOWS
    if (ret == SOCKET_ERROR)
#endif
#ifdef ____LINUX
        if (ret < 0)
#endif
    {
        YLOG_FATAL("In Socket::Bind(), bind() error")
    }
}

int connect(socket_t sockfd, const std::shared_ptr<IPAddress> &peerAddr) {
    return ::connect(sockfd, peerAddr->GetRawAddr(), peerAddr->GetRawAddrLen());
}

void close(socket_t sockfd) {
#ifdef ____LINUX
    auto ret = ::close(sockfd);
#endif
#ifdef ____WINDOWS
    auto ret = ::closesocket(sockfd);
#endif
    if (ret < 0) {
        YLOG_ERROR("In Socket::Close(), close error: {}", yy::util::GetLastErrorInfo())
    } else {
        YLOG_DEBUG("套接字<{}>已Close！", sockfd)
    }
}

void set_nonblocking(socket_t sockfd) {
#ifdef ____LINUX
    int old_option = ::fcntl(sockfd, F_GETFL);
    ::fcntl(sockfd, F_SETFL, old_option | O_NONBLOCK);
#endif

#ifdef ____WINDOWS
    unsigned long ul = 1;
    ioctlsocket(sockfd, FIONBIO, (unsigned long *) &ul);
#endif
}


SocketApiWrapper::socket_t
accept(socket_t sockfd, std::shared_ptr<IPAddress> outPeerAddr, bool isNewSockNonBlock) {
    int connfd;

    //FIXME 非阻塞Accept应该不断loop
#ifdef ____LINUX
    int flags = isNewSockNonBlock ? SOCK_CLOEXEC | SOCK_NONBLOCK : 0;
    if (outPeerAddr) {
        auto addrLen = outPeerAddr->GetRawAddrLen();
        connfd = ::accept4(sockfd, outPeerAddr->GetRawAddr(), &addrLen, flags);
    } else {
        connfd = ::accept4(sockfd, nullptr, nullptr, flags);
    }
#endif

#ifdef ____WINDOWS
    if (outPeerAddr) {
        auto addrLen = outPeerAddr->GetRawAddrLen();
        connfd = ::accept(sockfd, outPeerAddr->GetRawAddr(), &addrLen);
    } else {
        connfd = ::accept(sockfd, nullptr, nullptr);
    }

    if (connfd >= 0 and isNewSockNonBlock) {
        set_nonblocking(connfd);
    }
#endif

    if (has_socket_error(connfd)) {
        yy::util::ErrnoSaver errnoSaver;
        //! 因为accept需要被调用无数次，为保证程序的正常运行，所以需要区分暂时错误和致命错误
        switch (errnoSaver) {
            //! 暂时错误：忽略之
            case EAGAIN:
            case ECONNABORTED:
            case EINTR:
            case EPROTO:
            case EPERM:
            case EMFILE: {
                errno = errnoSaver;
                break;
            }
                //! 致命错误：终止错误
            case EBADF:
            case EFAULT:
            case EINVAL:
            case ENFILE:
            case ENOBUFS:
            case ENOMEM:
            case ENOTSOCK:
            case EOPNOTSUPP: {
                YLOG_FATAL("In Socket::Accept(), accpet() unexcepted error: {}", yy::util::GetErrorInfo(errnoSaver))
                break;
            }
            default: {
                YLOG_FATAL("In Socket::Accept(), accpet() unknown error: {}", yy::util::GetErrorInfo(errnoSaver))
                break;
            }
        }
    }
    return connfd;
}


void shutdown(socket_t sockfd, int how) {
    int ret = ::shutdown(sockfd, how);
    if (ret < 0) {
        // 用户连接reset后，再调用shutdown，则会造成该情况
        if (errno == ENOTCONN) {
            return;
        } else {
            YLOG_ERROR("In Socket::Shutdown(), shutdown() error: {}", yy::util::GetLastErrorInfo())
        }
    }
}

int get_socket_error(socket_t sockfd) {
    int optval;
    socklen_t optlen = static_cast<socklen_t>(sizeof optval);

    if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (char *) &optval, &optlen) < 0) {
        return get_last_socket_error();
    } else {
        return optval;
    }
}

bool is_self_connect(socket_t sockfd) {
    auto localAddr = GetLocalAddr(sockfd);
    auto peerAddr = GetPeerAddr(sockfd);

    return localAddr->GetFamily() == peerAddr->GetFamily() and localAddr->GetPort() == peerAddr->GetPort();
}


IPAddress::ptr GetLocalAddr(SocketApiWrapper::socket_t sockfd) {
    struct sockaddr_storage localAddr;
    socklen_t addrLen = sizeof localAddr;

    auto ret = ::getsockname(sockfd, (struct sockaddr *) (&localAddr), &addrLen);
    if (ret < 0) {
        YLOG_ERROR("In IPAddress::GetLocalAddr, ::getsockname() error: {}",
                   yy::util::StatusCode(errno).ToString().c_str());
        return nullptr;
    }

    IPAddress::ptr addr{};
    if (localAddr.ss_family == AF_INET) {
        addr = std::make_shared<yy::net::IPv4Address>((struct sockaddr_in *) &localAddr);
    } else {
        addr = std::make_shared<yy::net::IPv6Address>((struct sockaddr_in6 *) &localAddr);
    }

    return addr;
}

IPAddress::ptr GetPeerAddr(SocketApiWrapper::socket_t sockfd) {
    struct sockaddr_storage peerAddr;
    socklen_t addrLen = sizeof peerAddr;

    auto ret = ::getpeername(sockfd, (struct sockaddr *) (&peerAddr), &addrLen);
    if (ret < 0) {
        YLOG_ERROR("In IPAddress::GetPeerAddr, ::getpeername() error: {}",
                   yy::util::StatusCode(errno).ToString().c_str());
        return nullptr;
    }

    IPAddress::ptr addr{};
    if (peerAddr.ss_family == AF_INET) {
        addr = std::make_shared<yy::net::IPv4Address>((struct sockaddr_in *) &peerAddr);
    } else {
        addr = std::make_shared<yy::net::IPv6Address>((struct sockaddr_in6 *) &peerAddr);
    }

    return addr;
}


SocketResult recv(socket_t sockfd, void *ptr, size_t nbytes, int flags) {
#ifdef ____LINUX
    flags |= MSG_NOSIGNAL;
#endif
    auto ret = ::recv(sockfd, (char *) ptr, nbytes, flags);
    return {ret, get_last_socket_error()};
}

SocketApiWrapper::SocketResult send(socket_t sockfd, const void *ptr, size_t nbytes, int flags) {
#ifdef ____LINUX
    flags |= MSG_NOSIGNAL;
#endif
    auto ret = ::send(sockfd, (char *) ptr, nbytes, flags);
    return {ret, get_last_socket_error()};
}

SocketApiWrapper::SocketResult sendto(socket_t sockfd, const void *ptr, size_t nbytes, int flags, std::shared_ptr<IPAddress> peerAddr) {
    assert(peerAddr);
    auto ret = ::sendto(sockfd, (char *) ptr, nbytes, flags, peerAddr->GetRawAddr(), peerAddr->GetRawAddrLen());
    return {ret, get_last_socket_error()};
}

SocketApiWrapper::SocketResult recvfrom(socket_t sockfd, void *ptr, size_t nbytes, int flags, std::shared_ptr<IPAddress> peerAddr) {
    assert(peerAddr);
    auto addrLen = peerAddr->GetRawAddrLen();
    auto ret = ::recvfrom(sockfd, (char *) ptr, nbytes, flags, peerAddr->GetRawAddr(), &addrLen);
    return {ret, get_last_socket_error()};
}

SocketApiWrapper::SocketResult readv(socket_t sockfd, IOV_TYPE *iov, int iovcnt) {
#ifdef ____WINDOWS
    DWORD bytesRead;
    DWORD flags = 0;
    auto ret = WSARecv(sockfd, iov, iovcnt, &bytesRead, &flags, NULL, NULL);
    if (ret == SOCKET_ERROR) {
        return {-1, get_last_socket_error()};
    } else {
        return { static_cast<ssize_t>(bytesRead), 0 };
    }
#elif defined(____LINUX)
    auto ret = ::readv(sockfd, iov, iovcnt);
    return {ret, get_last_socket_error()};
#else
    #error Platform not supported
#endif
}

SocketApiWrapper::SocketResult readmsg(socket_t sockfd, IOV_TYPE *iov, int iovcnt, std::shared_ptr<IPAddress> peerAddr) {
    assert(peerAddr);

#ifdef ____WINDOWS
    DWORD bytesRead;
    DWORD flags = 0;
    auto addrLen = peerAddr->GetRawAddrLen();
    auto ret = WSARecvFrom(sockfd, iov, iovcnt, &bytesRead, &flags, peerAddr->GetRawAddr(), &addrLen, NULL, NULL);
    if (ret == SOCKET_ERROR) {
        return {-1, get_last_socket_error()};
    } else {
        return { static_cast<ssize_t>(bytesRead), 0 };
    }
#elif defined(____LINUX)
    struct msghdr msg = {};
    msg.msg_name = peerAddr->GetRawAddr();
    msg.msg_namelen = peerAddr->GetRawAddrLen();
    msg.msg_iov = iov;
    msg.msg_iovlen = iovcnt;

    auto ret = ::recvmsg(sockfd, &msg, 0);
    return {ret, get_last_socket_error()};
#else
    #error Platform not supported
#endif
}


}
