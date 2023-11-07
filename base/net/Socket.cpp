#include "Socket.h"
#include "log.h"
#include "status/Status.h"
#include "ErrnoSaver.h"
#include "SocketApiWrapper.h"




namespace yy::net {

template<typename T>
int SetOpt(SocketApiWrapper::socket_t sockfd, int SO_XXXX, const T &optval)
{
    return ::setsockopt(sockfd, SOL_SOCKET, SO_XXXX, (const char *)&optval, sizeof(optval));
}




Socket::Socket(SocketApiWrapper::socket_t sockfd, Socket::Type type, Socket::Family family, bool isNonblock)
        : m_socketfd(sockfd),
          m_IsNonblocking {isNonblock},
          m_type{type},
          m_family{family}
{ }


Socket::Socket(Type type, Family family, bool isNonblock)
    : m_IsNonblocking {isNonblock}, m_type{type}, m_family{family}
{
    m_socketfd = SocketApiWrapper::create_or_die((int)family, (int)type, isNonblock);
}



Socket::~Socket() {
    YLOG_TRACE("套接字<%d>被析构", m_socketfd);
    Close();


}

void Socket::Listen(int backlog) {
    SocketApiWrapper::listen_or_die(m_socketfd, backlog);
}

SocketApiWrapper::socket_t Socket::Accept(IPAddress::ptr &outPeerAddr, bool isNewSockNonBlock) {
    return SocketApiWrapper::accept(m_socketfd, outPeerAddr, isNewSockNonBlock);
}

void Socket::Bind(const IPAddress::ptr &localAddr) {
    SocketApiWrapper::bind_or_die(m_socketfd, localAddr);
}

void Socket::Connect(const IPAddress::ptr &peerAddr) {
    SocketApiWrapper::connect(m_socketfd, peerAddr);
}



void Socket::SetOpt_ReuseAddr(bool onoff) {
    int opt_val = onoff;
    SetOpt(m_socketfd, SO_REUSEADDR, opt_val);
}

void Socket::SetOpt_Linger(bool onoff, int timeout) {
    struct linger ling{};
    ling.l_onoff = onoff;
    ling.l_linger = timeout;
    SetOpt(m_socketfd, SO_LINGER, ling);
}

void Socket::SetOpt_KeepAlive(bool onoff) {
    int opt_val = onoff;
    SetOpt(m_socketfd, SO_KEEPALIVE, opt_val);
}

void Socket::SetOpt_ReusePort(bool onoff) {
    int opt_val = onoff;
    int ret = SetOpt(m_socketfd, SO_REUSEPORT, opt_val);
    if(ret < 0 and onoff) {
        YLOG_ERROR("In Socket::SetOpt_ReusePort(), setsockopt() error, %s",
                   util::StatusCode{errno}.ToString().c_str())
    }
}

void Socket::SetOpt_RecvBuf(int bufSize) {
    SetOpt(m_socketfd, SO_RCVBUF, bufSize);
}

void Socket::SetOpt_SendBuf(int bufSize) {
    SetOpt(m_socketfd, SO_SNDBUF, bufSize);
}


void Socket::Shutdown(ShutdownType how) {
    SocketApiWrapper::shutdown(m_socketfd, how);
}

void Socket::Shutdown_RD() { Shutdown(SHUT_RD); }

void Socket::Shutdown_WR() { Shutdown(SHUT_WR); }

void Socket::Close() {
    SocketApiWrapper::close(m_socketfd);
}


ssize_t Socket::Recv(void *ptr, size_t nbytes, int flags)
{
    ssize_t ret = ::recv(m_socketfd, ptr, nbytes, flags);
    return ret;
}

ssize_t Socket::Send(const void *ptr, size_t nbytes, int flags)
{
    ssize_t ret = ::send(m_socketfd, ptr, nbytes, flags);
    return ret;
}

ssize_t Socket::Sendto(const void *ptr, size_t nbytes, int flags, IPAddress::ptr peerAddr)
{
    return ::sendto(m_socketfd, ptr, nbytes, flags, peerAddr->GetRawAddr(), peerAddr->GetRawAddrLen());
}


ssize_t Socket::Recvfrom(void *ptr, size_t nbytes, int flags, IPAddress::ptr peerAddr)
{
    auto addrLen = peerAddr->GetRawAddrLen();
    return ::recvfrom(m_socketfd, ptr, nbytes, flags, peerAddr->GetRawAddr(), &addrLen);
}

void Socket::SetNonblocking() {
    SocketApiWrapper::set_nonblocking(m_socketfd);
}






} // yy::net










