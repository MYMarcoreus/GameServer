#ifndef NETBUFFER_H
#define NETBUFFER_H

#include "net_definations.h"
#include "socket_definations.h"
#include "RingBuffer.h"


namespace yy::net
{

class Socket;

class NetBuffer : public util::RingBuffer {
public:
    explicit NetBuffer(size_t _maxsize): RingBuffer(_maxsize) {}

    /*! Buffer不实现来自套接字Socket的recv任务，因为对于recv任务，存在ET和LT的区别，因此原样recv的错误，让其所有者TcpConnection实现 !*/
    bool RecvAllFromSocket(const std::unique_ptr<Socket> & sock, SocketApiWrapper::SocketResult & rst, const std::shared_ptr<IPAddress>& peerAddr);

    SocketApiWrapper::SocketResult SendAllToSocket(const std::unique_ptr<Socket> & sock, const std::shared_ptr<IPAddress>& peerAddr);
};

}

#endif //NETBUFFER_H
