#ifndef LINUXGAMESERVER_SOCKETAPIWRAPPER_H
#define LINUXGAMESERVER_SOCKETAPIWRAPPER_H

#include "noncopyable.h"
#include "socket_definations.h"
#include <memory>


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
socket_t accept(socket_t sockfd, std::shared_ptr<IPAddress> & outPeerAddr, bool isNewSockNonBlock);
void     close(socket_t sockfd);
void     shutdown (socket_t sockfd, int how);
void     set_nonblocking(socket_t sockfd);
int      get_socket_error(socket_t sockfd);
bool     is_self_connect(socket_t sockfd);



ssize_t recv(socket_t sockfd, void *ptr, size_t nbytes, int flags);
ssize_t send(socket_t sockfd, const void *ptr, size_t nbytes, int flags);
ssize_t sendto(socket_t sockfd, const void *ptr, size_t nbytes, int flags, std::shared_ptr<IPAddress> peerAddr);
ssize_t recvfrom(socket_t sockfd, void *ptr, size_t nbytes, int flags, std::shared_ptr<IPAddress> peerAddr);



std::shared_ptr<IPAddress> GetLocalAddr(SocketApiWrapper::socket_t sockfd);
std::shared_ptr<IPAddress> GetPeerAddr (SocketApiWrapper::socket_t sockfd);

}



#endif //LINUXGAMESERVER_SOCKETAPIWRAPPER_H
