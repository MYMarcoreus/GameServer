#include"IPAddress.h"
#include"log.h"
#include"status/Status.h"
#include "net_definations.h"

#include<arpa/inet.h>
#include<cstring>


namespace yy::net {

IPv4Address::IPv4Address() : IPv4Address(0, 0) {}

IPv4Address::IPv4Address(uint16_t port) : IPv4Address(INADDR_ANY, port) {}

IPv4Address::IPv4Address(const std::string &ipv4_str): IPv4Address(ipv4_str, 0) { }


IPv4Address::IPv4Address(const std::string & ipv4_str, uint16_t port): m_address{}
{
    int ret = ::inet_pton(AF_INET, ipv4_str.c_str(), &m_address.sin_addr.s_addr);
    if (ret == 0) {
        YLOG_FATAL("inet_pton() error: invalid format of ipv4 address.")
        throw std::invalid_argument("invalid format of ipv4 address.");
    } else if (ret == -1 and errno == EAFNOSUPPORT) {
        YLOG_FATAL("inet_pton() error, invalid address family: %s.", strerror(errno))
        throw std::invalid_argument("invalid address family");
    }
    m_address.sin_port = ::htons(port);
    m_address.sin_family = AF_INET;
}

IPv4Address::IPv4Address(uint32_t ipv4, uint16_t port): m_address{}
{
    m_address.sin_addr.s_addr = ::htonl(ipv4);
    m_address.sin_port = ::htons(port);
    m_address.sin_family = AF_INET;
}






IPv6Address::IPv6Address(const std::string &ipv6_str, uint16_t port) : m_address{} {
    //todo
}

std::string IPv6Address::GetIPStr() const {
    // todo
    return {};
}

uint32_t IPv6Address::GetIP() const {
    // todo
    return 0;
}

std::string IPv6Address::ToString() const {
    // todo
    return std::string();
}

std::string IPv6Address::GetPortStr() const {
    // todo
    return std::string();
}


IPAddress::ptr IPAddress::GetLocalAddr(SocketApiWrapper::socket_t sockfd) {
    struct sockaddr_storage localAddr;
    socklen_t addrLen = sizeof localAddr;

    auto ret = ::getsockname(sockfd, (struct sockaddr*)(&localAddr), &addrLen);
    if(ret < 0) {
        YLOG_ERROR("In IPAddress::GetLocalAddr, ::getsockname() error: %s",
                   util::StatusCode(errno).ToString().c_str());
        return nullptr;
    }

    IPAddress::ptr addr{};
    if(localAddr.ss_family == AF_INET) {
        addr = std::make_shared<IPv4Address>((struct sockaddr_in *)&localAddr);
    } else {
        addr = std::make_shared<IPv6Address>((struct sockaddr_in6 *)&localAddr);
    }

    return addr;
}

IPAddress::ptr IPAddress::GetPeerAddr(SocketApiWrapper::socket_t sockfd) {
    struct sockaddr_storage peerAddr;
    socklen_t addrLen = sizeof peerAddr;

    auto ret = ::getpeername(sockfd, (struct sockaddr*)(&peerAddr), &addrLen);
    if(ret < 0) {
        YLOG_ERROR("In IPAddress::GetPeerAddr, ::getpeername() error: %s",
                   util::StatusCode(errno).ToString().c_str());
        return nullptr;
    }

    IPAddress::ptr addr{};
    if(peerAddr.ss_family == AF_INET) {
        addr = std::make_shared<IPv4Address>((struct sockaddr_in *)&peerAddr);
    } else {
        addr = std::make_shared<IPv6Address>((struct sockaddr_in6 *)&peerAddr);
    }

    return addr;
}
}