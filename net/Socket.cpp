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
    m_socketfd = SocketApiWrapper::create_or_die((sa_family_t)family, (__socket_type)type, isNonblock);
}



Socket::~Socket() {
    YLOG_TRACE("套接字<{}>被析构", m_socketfd);
    Close();


}

void Socket::Listen(int backlog) {
    SocketApiWrapper::listen_or_die(m_socketfd, backlog);
}

std::unordered_map<SocketApiWrapper::socket_t, IPAddressPtr>
Socket::AcceptAll(const bool isNewSockNonBlock) {
    switch (this->GetFamily()) {
        case Socket::Family::IPv4:
            return SocketApiWrapper::acceptAll<IPv4Address>(m_socketfd, isNewSockNonBlock);
        case Socket::Family::IPv6:
            return SocketApiWrapper::acceptAll<IPv6Address>(m_socketfd, isNewSockNonBlock);
        default:
            throw std::invalid_argument("wrong socket family of accept socket");
    }
}


void Socket::Bind(const IPAddressPtr &localAddr) {
    SocketApiWrapper::bind_or_die(m_socketfd, localAddr);
}

void Socket::Connect(const IPAddressPtr &peerAddr) {
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

void Socket::SetOpt_KeepAlive(const bool onoff) {
    const int opt_val = onoff;
    SetOpt(m_socketfd, SO_KEEPALIVE, opt_val);
}

//! windows没有SO_REUSEPORT
// void Socket::SetOpt_ReusePort(bool onoff) {
//     int opt_val = onoff;
//     int ret = SetOpt(m_socketfd, SO_REUSEPORT, opt_val);
//     if(ret < 0 and onoff) {
//         YLOG_ERROR("In Socket::SetOpt_ReusePort(), setsockopt() error, {}", util::StatusCode{errno}.ToString())
//     }
// }

void Socket::SetOpt_RecvBuf(int bufSize) {
    SetOpt(m_socketfd, SO_RCVBUF, bufSize);
}

void Socket::SetOpt_SendBuf(int bufSize) {
    SetOpt(m_socketfd, SO_SNDBUF, bufSize);
}


void Socket::Shutdown(ShutdownType how) {
    SocketApiWrapper::shutdown(m_socketfd, (int)how);
}

void Socket::Shutdown_RD() { Shutdown(ShutdownType::eShut_RD); }

void Socket::Shutdown_WR() { Shutdown(ShutdownType::eShut_WR); }

void Socket::Close() {
    SocketApiWrapper::close(m_socketfd);
}


SocketApiWrapper::SocketResult Socket::Recv(void *ptr, const size_t nbytes, const int flags)
{
    return SocketApiWrapper::recv(m_socketfd, ptr, nbytes, flags);
}

SocketApiWrapper::SocketResult Socket::Send(const void *ptr, const size_t nbytes, const int flags)
{
    return SocketApiWrapper::send(m_socketfd, ptr, nbytes, flags);
}

SocketApiWrapper::SocketResult Socket::Sendto(const void *ptr, const size_t nbytes, const int flags, const IPAddress::ptr& peerAddr)
{
    return SocketApiWrapper::sendto(m_socketfd, ptr, nbytes, flags, peerAddr);
}


SocketApiWrapper::SocketResult Socket::Recvfrom(void *ptr, const size_t nbytes, const int flags, const IPAddress::ptr& peerAddr)
{
    return SocketApiWrapper::recvfrom(m_socketfd, ptr, nbytes, flags, peerAddr);
}

IPAddress::ptr Socket::GetLocalAddr()
{
    return SocketApiWrapper::GetLocalAddr(m_socketfd);
}

IPAddress::ptr Socket::GetPeerAddr()
{
    return SocketApiWrapper::GetPeerAddr(m_socketfd);
}

void Socket::SetNonblocking() {
    SocketApiWrapper::set_nonblocking(m_socketfd);
}

SocketApiWrapper::SocketResult Socket::Readv(IOV_TYPE *iov, int iovcnt) {
    return SocketApiWrapper::readv(m_socketfd, iov, iovcnt);
}

SocketApiWrapper::SocketResult Socket::Readmsg(IOV_TYPE *iov, int iovcnt, IPAddress::ptr peerAddr) {
    return SocketApiWrapper::recvmsg(m_socketfd, iov, iovcnt, peerAddr);
}

SocketApiWrapper::SocketResult Socket::Writev(IOV_TYPE* iov, int iovcnt)
{
    return SocketApiWrapper::writev(m_socketfd, iov, iovcnt);
}

SocketApiWrapper::SocketResult Socket::Sendmsg(IOV_TYPE* iov, int iovcnt, IPAddress::ptr peerAddr)
{
    return SocketApiWrapper::sendmsg(m_socketfd, iov, iovcnt, peerAddr);
}
} // yy::net
