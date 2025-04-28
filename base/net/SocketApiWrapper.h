#ifndef LINUXGAMESERVER_SOCKETAPIWRAPPER_H
#define LINUXGAMESERVER_SOCKETAPIWRAPPER_H

#include "noncopyable.h"
#include "socket_definations.h"
#include "IPAddress.h"
#include "util_functions.h"
#include "log.h"
#include <memory>
#include <unordered_map>
#include <type_traits>


namespace yy::net {
class IPAddress;
}


namespace SocketApiWrapper
{

using yy::net::IPAddress;

socket_t create_or_die(sa_family_t family = AF_INET, __socket_type type = SOCK_STREAM, bool isNonblock = true);

socket_t create_tcp_or_die(bool isNonblock);
socket_t create_udp_or_die(bool isNonblock);
void     listen_or_die(socket_t sockfd, int backlog = SOMAXCONN);
void     bind_or_die(socket_t sockfd, const std::shared_ptr<IPAddress> & localAddr);
int      connect(socket_t sockfd, const std::shared_ptr<IPAddress> & peerAddr);
socket_t accept(socket_t sockfd, std::shared_ptr<IPAddress> outPeerAddr, bool isNewSockNonBlock);



void     close(socket_t sockfd);
void     shutdown (socket_t sockfd, int how);
void     set_nonblocking(socket_t sockfd);
int      get_socket_error(socket_t sockfd);
bool     is_self_connect(socket_t sockfd);



ssize_t recv(socket_t sockfd, void *ptr, size_t nbytes, int flags);
ssize_t send(socket_t sockfd, const void *ptr, size_t nbytes, int flags);
ssize_t sendto(socket_t sockfd, const void *ptr, size_t nbytes, int flags, std::shared_ptr<IPAddress> peerAddr);
ssize_t recvfrom(socket_t sockfd, void *ptr, size_t nbytes, int flags, std::shared_ptr<IPAddress> peerAddr);

ssize_t readv(socket_t sockfd, IOV_TYPE *iov, int iovcnt);



extern std::shared_ptr<IPAddress> GetLocalAddr(SocketApiWrapper::socket_t sockfd);
extern std::shared_ptr<IPAddress> GetPeerAddr (SocketApiWrapper::socket_t sockfd);

template<class IPADDR> requires requires {
    requires std::is_base_of_v<IPAddress, IPADDR>;
}
std::unordered_map<socket_t, std::shared_ptr<IPAddress>> acceptAll(socket_t sockfd, bool isNewSockNonBlock)
{
    std::unordered_map<socket_t, std::shared_ptr<IPAddress>> Connfd2Addrs;
#ifdef ____LINUX
    while (true) {
        IPAddress::ptr outPeerAddr = std::make_shared<IPADDR>();
        auto addrLen = outPeerAddr->GetRawAddrLen();
        int flags = isNewSockNonBlock ? SOCK_CLOEXEC | SOCK_NONBLOCK : 0;
        socket_t connfd = ::accept4(sockfd, outPeerAddr->GetRawAddr(), &addrLen, flags);

        if (connfd >= 0) {
            Connfd2Addrs[connfd] = outPeerAddr;
        } else {
            yy::util::ErrnoSaver errnoSaver;
            int err = errnoSaver();
            if (err == EAGAIN || err == EWOULDBLOCK) {
                break;
            } else if (err == EINTR) {
                continue;
            } else {
                YLOG_FATAL("In Socket::acceptAll(), accept() error: {}", yy::util::GetErrorInfo(err));
                break;
            }
        }
    }
#endif

#ifdef ____WINDOWS
    while (true) {
        IPAddress::ptr outPeerAddr = std::make_shared<IPADDR>();
        auto addrLen = outPeerAddr->GetRawAddrLen();
        socket_t connfd = ::accept(sockfd, outPeerAddr->GetRawAddr(), &addrLen);
        if (connfd != INVALID_SOCKET) {
            if (isNewSockNonBlock) {
                set_nonblocking(connfd);
            }
            Connfd2Addrs[connfd] = outPeerAddr;
        } else {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) {
                break;
            } else if (err == WSAEINTR) {
                continue;
            } else {
                YLOG_FATAL("In Socket::acceptAll(), accept() error: {}", yy::util::GetErrorInfo(err));
                break;
            }
        }
    }
#endif

    return std::move(Connfd2Addrs);
}



}



#endif //LINUXGAMESERVER_SOCKETAPIWRAPPER_H

