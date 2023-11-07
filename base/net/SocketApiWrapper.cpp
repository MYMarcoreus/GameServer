#include "SocketApiWrapper.h"
#include "IPAddress.h"
#include "log.h"
#include "status/Status.h"
#include "ErrnoSaver.h"
#include <unistd.h>
#include <fcntl.h>


namespace SocketApiWrapper {


socket_t create_or_die(sa_family_t family, __socket_type type, bool isNonblock) {
#ifdef ____LINUX
    int realType = isNonblock ? (int) type | SOCK_NONBLOCK | SOCK_CLOEXEC : (int) type;
    socket_t socketfd = ::socket(family, realType, 0);
#endif

#ifdef ____WINDOWS
    WORD wsaword;
    WSADATA wsadata;
    wsaword = MAKEWORD(2,2);
    uint64_t iError = ::WSAStartup(wsaword, &wsadata);
    if (iError != NOERROR) {
        return INVALID_SOCKET;
    }
    if ((2 != LOBYTE(wsadata.wVersion)) || (2 != LOBYTE(wsadata.wHighVersion))) {
        ::WSACleanup();
        return INVALID_SOCKET;
    }
    socket_t sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == INVALID_SOCKET) {
        ::WSACleanup();
        return INVALID_SOCKET;
    }

    set_nonblocking(sockfd);
#endif

    if (sockfd <= 0) {
        YLOG_FATAL("In Socket::Socket(), socket() error: {}", yy::util::StatusCode{errno}.ToString())
    }

    return sockfd;
}

void listen_or_die(socket_t sockfd, int backlog) {
    int ret = ::listen(sockfd, backlog);
    if (ret < 0) {
        YLOG_FATAL("In Socket::StartListen(), listen() error: {}", yy::util::StatusCode{errno}.ToString())
    }
}

void bind_or_die(socket_t sockfd, const std::shared_ptr<IPAddress> &localAddr) {
    int ret = ::bind(sockfd, localAddr->GetRawAddr(), localAddr->GetRawAddrLen());
    if (ret < 0) {
        YLOG_FATAL("In Socket::Bind(), bind() error")
    }
}

int connect(socket_t sockfd, const std::shared_ptr<IPAddress> &peerAddr) {
    return ::connect(sockfd, peerAddr->GetRawAddr(), peerAddr->GetRawAddrLen());
}

void
close(socket_t sockfd) {
    if (sockfd >= 0) {
        auto ret = ::close(sockfd);
        if (ret < 0) {
            YLOG_ERROR("In Socket::Close(), close error: %s", yy::util::StatusCode(errno).ToString().c_str())
        } else {
            YLOG_DEBUG("套接字<%d>已Close！", sockfd)
        }
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
                YLOG_FATAL("In Socket::Accept(), accpet4() unexcepted error: {}", yy::util::StatusCode{errnoSaver}.ToString())
                break;
            }
            default: {
                YLOG_FATAL("In Socket::Accept(), accpet4() unknown error: {}", yy::util::StatusCode{errnoSaver}.ToString())
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
            YLOG_ERROR("In Socket::Shutdown(), shutdown() error: {}", yy::util::StatusCode{errno}.ToString())
        }
    }
}

int get_socket_error(socket_t sockfd) {
    int optval;
    socklen_t optlen = static_cast<socklen_t>(sizeof optval);

    if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
        return errno;
    } else {
        return optval;
    }
}

bool is_self_connect(socket_t sockfd) {
    auto localAddr = yy::net::IPAddress::GetLocalAddr(sockfd);
    auto peerAddr = yy::net::IPAddress::GetPeerAddr(sockfd);

    return localAddr->GetFamily() == peerAddr->GetFamily() and localAddr->GetPort() == peerAddr->GetPort();
}


}