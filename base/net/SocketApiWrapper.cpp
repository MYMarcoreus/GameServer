#include "SocketApiWrapper.h"
#include "IPAddress.h"
#include "log.h"
#include "status/Status.h"
#include "ErrnoSaver.h"
#include <fcntl.h>



#include "net_definations.h"

namespace SocketApiWrapper {


socket_t create_or_die(sa_family_t family, __socket_type type, bool isNonblock) {
#ifdef ____LINUX
    int realType = isNonblock ? (int) type | SOCK_NONBLOCK | SOCK_CLOEXEC : (int) type;
    socket_t sockfd = ::socket(family, realType, 0);
    if (sockfd < 0) {
        YLOG_FATAL("In Socket::Socket(), socket() error: {}", yy::util::GetLastErrorInfo());
    }
#endif

#ifdef ____WINDOWS
    // WORD wsaword;
    // WSADATA wsadata;
    // wsaword = MAKEWORD(2,2);
    // uint64_t iError = ::WSAStartup(wsaword, &wsadata);
    // if (iError != NOERROR) {
    //     return INVALID_SOCKET;
    // }
    // if ((2 != LOBYTE(wsadata.wVersion)) || (2 != LOBYTE(wsadata.wHighVersion))) {
    //     ::WSACleanup();
    //     return INVALID_SOCKET;
    // }

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Failed to initialize Winsock." << std::endl;
        return 1;
    }


    socket_t sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == INVALID_SOCKET) {
        ::WSACleanup();
        YLOG_FATAL("In Socket::Socket(), socket() error: {}", yy::util::GetLastErrorInfo())
    }

    set_nonblocking(sockfd);
#endif

    return sockfd;
}

socket_t create_tcp_or_die(bool isNonblock) {
    return create_or_die(AF_INET, SOCK_STREAM, isNonblock);
}

socket_t create_udp_or_die(bool isNonblock) {
    return create_or_die(AF_INET6, SOCK_DGRAM, isNonblock);
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
    ioctlsocket(sockfd, FIONBIO, (unsigned long *)&ul);
#endif
}


SocketApiWrapper::socket_t
accept(socket_t sockfd, std::shared_ptr<IPAddress> &outPeerAddr, bool isNewSockNonBlock) {

    auto addrLen = outPeerAddr->GetRawAddrLen();

#ifdef ____LINUX
    int flags = isNewSockNonBlock ? SOCK_CLOEXEC | SOCK_NONBLOCK : 0;
    //! 使用accept4直接设置接受的套接字为非阻塞套接字
    int connfd = ::accept4(sockfd, outPeerAddr->GetRawAddr(), &addrLen, flags);
#endif

#ifdef ____WINDOWS


    int connfd = ::accept(sockfd, outPeerAddr->GetRawAddr(), &addrLen);
    if (connfd >= 0)
    {
        set_nonblocking(sockfd);
    }
#endif
    if (connfd < 0) {
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

    if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (char*)&optval, &optlen) < 0) {
        return errno;
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

    auto ret = ::getsockname(sockfd, (struct sockaddr*)(&localAddr), &addrLen);
    if(ret < 0) {
        YLOG_ERROR("In IPAddress::GetLocalAddr, ::getsockname() error: {}",
                   yy::util::StatusCode(errno).ToString().c_str());
        return nullptr;
    }

    IPAddress::ptr addr{};
    if(localAddr.ss_family == AF_INET) {
        addr = std::make_shared<yy::net::IPv4Address>((struct sockaddr_in *)&localAddr);
    } else {
        addr = std::make_shared<yy::net::IPv6Address>((struct sockaddr_in6 *)&localAddr);
    }

    return addr;
}

IPAddress::ptr GetPeerAddr(SocketApiWrapper::socket_t sockfd) {
    struct sockaddr_storage peerAddr;
    socklen_t addrLen = sizeof peerAddr;

    auto ret = ::getpeername(sockfd, (struct sockaddr*)(&peerAddr), &addrLen);
    if(ret < 0) {
        YLOG_ERROR("In IPAddress::GetPeerAddr, ::getpeername() error: {}",
                   yy::util::StatusCode(errno).ToString().c_str());
        return nullptr;
    }

    IPAddress::ptr addr{};
    if(peerAddr.ss_family == AF_INET) {
        addr = std::make_shared<yy::net::IPv4Address>((struct sockaddr_in *)&peerAddr);
    } else {
        addr = std::make_shared<yy::net::IPv6Address>((struct sockaddr_in6 *)&peerAddr);
    }

    return addr;
}

ssize_t recv(socket_t sockfd, void *ptr, size_t nbytes, int flags) {
#ifdef ____LINUX
    flags |= MSG_NOSIGNAL;
#endif
    auto ret = ::recv(sockfd, (char *)ptr, nbytes, flags);
    return ret;
}

ssize_t send(socket_t sockfd, const void *ptr, size_t nbytes, int flags) {
#ifdef ____LINUX
    flags |= MSG_NOSIGNAL;
#endif
    auto ret = ::send(sockfd, (char *)ptr, nbytes, flags);
    return ret;
}

ssize_t sendto(socket_t sockfd, const void *ptr, size_t nbytes, int flags, std::shared_ptr<IPAddress> peerAddr) {
    return ::sendto(sockfd, (char *)ptr, nbytes, flags, peerAddr->GetRawAddr(), peerAddr->GetRawAddrLen());
}

ssize_t recvfrom(socket_t sockfd, void *ptr, size_t nbytes, int flags, std::shared_ptr<IPAddress> peerAddr) {
    auto addrLen = peerAddr->GetRawAddrLen();
    return ::recvfrom(sockfd, (char *)ptr, nbytes, flags, peerAddr->GetRawAddr(), &addrLen);
}

ssize_t readv(socket_t sockfd, IOV_TYPE * iov, int iovcnt) {
#ifdef ____MSVC
    DWORD bytesRead;
    DWORD flags = 0;
    if (WSARecv(sockfd, iov, iovcnt, &bytesRead, &flags, NULL, NULL))
    {
        if (GetLastError() == WSAECONNABORTED)
            //close
            return  0;
        else
            //error
            return -1;
    }
    else
        return bytesRead;
#else
    return ::readv(sockfd, iov, iovcnt);
#endif
}


}
