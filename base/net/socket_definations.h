#ifndef LINUXGAMESERVER_SOCKET_DEFINATIONS_H
#define LINUXGAMESERVER_SOCKET_DEFINATIONS_H


#include "cross_platform_defines.h"


#ifdef ____LINUX
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h> // gettimeofday
#include <unistd.h>   // readlink
#endif

#ifdef ____WINDOWS
#include <winsock2.h>
#include <ws2tcpip.h>

using sa_family_t = int;
using __socket_type = int;

#endif




namespace SocketApiWrapper {

#ifdef ____WINDOWS
using socket_t = SOCKET;
#endif

#ifdef ____LINUX
using socket_t = int;
#endif

}


#endif //LINUXGAMESERVER_SOCKET_DEFINATIONS_H

