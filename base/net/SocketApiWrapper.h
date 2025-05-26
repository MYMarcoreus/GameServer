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
#include <cstdint>  // for int64_t



namespace yy::net {
class IPAddress;
}


namespace yy::SocketApiWrapper
{

using yy::net::IPAddress;


socket_t create_or_die(sa_family_t family = AF_INET, __socket_type type = SOCK_STREAM, bool isNonblock = true);

socket_t create_tcp_or_die(bool isNonblock);
socket_t create_udp_or_die(bool isNonblock);
void     listen_or_die(socket_t sockfd, int backlog = SOMAXCONN);
void     bind_or_die(socket_t sockfd, const std::shared_ptr<IPAddress> & localAddr);
SocketApiWrapper::SocketResult connect(const socket_t sockfd, const std::shared_ptr<IPAddress> & peerAddr);

void     close(socket_t sockfd);
void     shutdown (socket_t sockfd, int how);
void     set_nonblocking(socket_t sockfd);
int      get_socket_error(socket_t sockfd);
int64_t  get_last_socket_error();
bool     is_self_connect(socket_t sockfd);



SocketResult recv(socket_t sockfd, void *ptr, size_t nbytes, int flags);
SocketApiWrapper::SocketResult send(socket_t sockfd, const void *ptr, size_t nbytes, int flags);
SocketApiWrapper::SocketResult sendto(socket_t sockfd, const void *ptr, size_t nbytes, int flags, std::shared_ptr<IPAddress> peerAddr);
SocketApiWrapper::SocketResult recvfrom(socket_t sockfd, void *ptr, size_t nbytes, int flags, std::shared_ptr<IPAddress> peerAddr);

SocketApiWrapper::SocketResult readv(socket_t sockfd, IOV_TYPE *iov, int iovcnt);
SocketApiWrapper::SocketResult readmsg(socket_t sockfd, IOV_TYPE *iov, int iovcnt, std::shared_ptr<IPAddress> peerAddr);



extern std::shared_ptr<IPAddress> GetLocalAddr(SocketApiWrapper::socket_t sockfd);
extern std::shared_ptr<IPAddress> GetPeerAddr (SocketApiWrapper::socket_t sockfd);

template<class IPADDR> requires requires {
    requires std::is_base_of_v<IPAddress, IPADDR>;
}
std::unordered_map<socket_t, std::shared_ptr<IPAddress>>
acceptAll(socket_t sockfd, bool isNewSockNonBlock)
{
    std::unordered_map<socket_t, std::shared_ptr<IPAddress>> Connfd2Addrs;

    while (true)
    {
        IPAddress::ptr outPeerAddr = std::make_shared<IPADDR>();
        auto addrLen = outPeerAddr->GetRawAddrLen();
#ifdef ____WINDOWS
        socket_t connfd = ::accept(sockfd, outPeerAddr->GetRawAddr(), &addrLen);
#elif defined(____LINUX)
        int flags = isNewSockNonBlock ? SOCK_CLOEXEC | SOCK_NONBLOCK : 0;
        socket_t connfd = ::accept4(sockfd, outPeerAddr->GetRawAddr(), &addrLen, flags);
#else
    #error Platform not supported
#endif

        SocketResult rst(connfd, get_last_socket_error());

        if (rst.HasError()) {
            auto err = rst.ErrorCode();
            if (err == SocketError::eAgain) {
                break;
            } else if (err == SocketError::eInterrupted) {
                continue;
            } else {
                YLOG_FATAL("In Socket::acceptAll(), accept() error: {}", rst.GetErrorInfo());
                break;
            }
        } else {
#ifdef ____WINDOWS
            if (isNewSockNonBlock) set_nonblocking(connfd);
#endif
            Connfd2Addrs[connfd] = outPeerAddr;
        }
    }

    return std::move(Connfd2Addrs);
}



}



#endif //LINUXGAMESERVER_SOCKETAPIWRAPPER_H

