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
void     listen_or_die(socket_t sockfd, int backlog = SOMAXCONN);
void     bind_or_die(socket_t sockfd, const std::shared_ptr<IPAddress> & localAddr);
int      connect(socket_t sockfd, const std::shared_ptr<IPAddress> & peerAddr);
socket_t accept(socket_t sockfd, std::shared_ptr<IPAddress> & outPeerAddr, bool isNewSockNonBlock);
void     close(socket_t sockfd);
void     shutdown (socket_t sockfd, int how);
void     set_nonblocking(socket_t sockfd);
int      get_socket_error(socket_t sockfd);
bool     is_self_connect(socket_t sockfd);

}



#endif //LINUXGAMESERVER_SOCKETAPIWRAPPER_H
