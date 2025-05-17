#ifndef LINUXGAMESERVER_SOCKET_H
#define LINUXGAMESERVER_SOCKET_H

#ifdef ____LINUX
#include <sys/socket.h>
#endif

#ifdef ____WINDOWS
#include <winsock2.h>
#endif

#include "noncopyable.h"
#include "net_definations.h"
#include "IPAddress.h"






namespace yy::net {

class IPAddress;

///@brief 套接字
class Socket: public util::noncopyable {
public:

    enum class Type
    {
        TCP = SOCK_STREAM,
        UDP = SOCK_DGRAM
    };

    enum class Family{
        /// IPv4 socket
        IPv4 = AF_INET,
        /// IPv6 socket
        IPv6 = AF_INET6
    };

    enum class ShutdownType {
#ifdef ____LINUX
        eShut_RD   = SHUT_RD,
        eShut_WR   = SHUT_WR,
        eShut_RDWR = SHUT_RDWR,
#endif

#ifdef ____WINDOWS
        eShut_RD   = SD_RECEIVE,
        eShut_WR   = SD_SEND,
        eShut_RDWR = SD_BOTH,
#endif
    };

    ///@brief 供Accept接受套接字之后初始化连接套接字
    Socket(SocketApiWrapper::socket_t sockfd, Type type, Family family, bool isNonblock = true);

    ///@brief 创建一个套接字（如监听套接字）
    Socket(Type type, Family family, bool isNonblock = true);

    ///@brief 析构时关闭套接字
    ~Socket();

    Type              GetType()   const { return m_type; }
    Family            GetFamily() const { return m_family; }
    SocketApiWrapper::socket_t GetFD()     const { return m_socketfd; }
    bool IsNonblocking() const { return m_IsNonblocking; }
    void SetNonblocking();
    void SetOpt_ReuseAddr(bool onoff);
    void SetOpt_KeepAlive(bool onoff);
    // void SetOpt_ReusePort(bool onoff);
    void SetOpt_Linger   (bool onoff, int timeout = 0);
    void SetOpt_RecvBuf(int bufSize);
    void SetOpt_SendBuf(int bufSize);


    ///@brief 设置为监听套接字，出错则终止程序
    void Listen(int backlog = SOMAXCONN);

    ///@brief 接受连接，并返回连接套接字；若出现致命错误，则终止程序，否则跳过
    SocketApiWrapper::socket_t Accept(IPAddressPtr &outPeerAddr, bool isNewSockNonBlock);

    std::unordered_map<SocketApiWrapper::socket_t, IPAddressPtr> AcceptAll(bool isNewSockNonBlock);

    ///@brief 绑定本地套接字，出错则终止程序
    void Bind(const IPAddressPtr &localAddr);

    void Connect(const IPAddressPtr &peerAddr);

    void Close();

/* TCP的I / O函数 */
    //! NOTE：本函数不处理返回值，需要调用者自行处理（如判断是否为ET模式）
    SocketApiWrapper::SocketResult Recv(void *ptr, size_t nbytes, int flags = 0);

    SocketApiWrapper::SocketResult Send(const void *ptr, size_t nbytes, int flags = 0);

    SocketApiWrapper::SocketResult Readv(IOV_TYPE *iov, int iovcnt);

    SocketApiWrapper::SocketResult Readmsg(IOV_TYPE *iov, int iovcnt, IPAddress::ptr peerAddr);

/* UDP的I / O函数 */
    //! NOTE：本函数不处理返回值，需要调用者自行处理（如判断是否为ET模式）
    SocketApiWrapper::SocketResult Recvfrom(void *ptr, size_t nbytes, int flags, IPAddress::ptr peerAddr);

    SocketApiWrapper::SocketResult Sendto(const void *ptr, size_t nbytes, int flags, IPAddress::ptr peerAddr);


    ///@brief 关闭套接字读写端
    void Shutdown(ShutdownType how = ShutdownType::eShut_RDWR);

    ///@brief 断开输入流(读端)
    void Shutdown_RD();

    ///@brief 断开输出流(写端)，使得套接字处于半关闭的状态
    void Shutdown_WR();

private:
    SocketApiWrapper::socket_t m_socketfd;
    Type                       m_type;
    Family                     m_family;
    bool                       m_IsNonblocking;
};

} // yy::net















#endif //LINUXGAMESERVER_SOCKET_H

