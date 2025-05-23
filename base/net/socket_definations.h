#ifndef LINUXGAMESERVER_SOCKET_DEFINATIONS_H
#define LINUXGAMESERVER_SOCKET_DEFINATIONS_H


#include "cross_platform_defines.h"




#ifdef ____WINDOWS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cassert>
using sa_family_t = int;
using __socket_type = int;
#elif defined(____LINUX)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h> // gettimeofday
#include <unistd.h>   // readlink
#include <cassert>

#else
    #error Platform not supported
#endif



namespace yy::SocketApiWrapper {

//! 网络错误
enum class SocketError {
    eAgain,
    eInterrupted,
    eConnectionReset,   // ECONNRESET: Connection reset by peer
    eNotConnected,
    eConnectionAborted,
    eConnectionRefused,
    eMsgSize,
    eUnknown,
};

#ifdef ____WINDOWS
using socket_t = SOCKET;
#elif defined(____LINUX)
using socket_t = int;
#else
    #error Platform not supported
#endif


struct SocketResult {
public:
    SocketResult(int64_t rst = 0, int64_t err = 0) : result(rst), errorCode(err) { }
    [[nodiscard]] int64_t & Result() {
        assert(HasNoError());
        return result;
    }
    [[nodiscard]] SocketError ErrorCode() const {
    #ifdef ____WINDOWS
        switch (errorCode) {
            case WSAEWOULDBLOCK:    return SocketError::eAgain;
            case WSAEINTR:          return SocketError::eInterrupted;
            case WSAENOTCONN:       return SocketError::eNotConnected;
            case WSAECONNRESET:     return SocketError::eConnectionReset;
            case WSAECONNABORTED:   return SocketError::eConnectionAborted;
            case WSAECONNREFUSED:   return SocketError::eConnectionRefused;
            case WSAEMSGSIZE:       return SocketError::eMsgSize;
            default:                return SocketError::eUnknown;
        }
    #elif defined(____LINUX)
        switch (errorCode) {
            // case EAGAIN:
            case EWOULDBLOCK:       return SocketError::eAgain;
            case EINTR:             return SocketError::eInterrupted;
            case ENOTCONN:          return SocketError::eNotConnected;
            case EPIPE:
            case ECONNRESET:        return SocketError::eConnectionReset;
            case ECONNABORTED:      return SocketError::eConnectionAborted;
            case ECONNREFUSED:      return SocketError::eConnectionRefused;
            case EMSGSIZE:          return SocketError::eMsgSize;
            default:                return SocketError::eUnknown;
        }
    #endif
    }


    bool HasError() const;
    bool HasNoError() const { return not HasError(); };

    std::string GetErrorInfo() const;




private:
    int64_t result;   // 成功返回的字节数，现在是 int64_t
    int64_t errorCode;    // 出错时保存 errno 或 WSAGetLastError
};



}


#endif //LINUXGAMESERVER_SOCKET_DEFINATIONS_H

