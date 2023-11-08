#ifndef ____USER_BASE_DATA_H
#define ____USER_BASE_DATA_H

#include <cstring>
#include <atomic>
#include "GameProtocol.h"
#include "Socket.h"
#include "IPAddress.h"
#include "UserBuffer.h"
#include <google/protobuf/message.h>


#define return_if(condition) if(condition) return
#define continue_if(condition) if(condition) continue


namespace yy::core {

/// @brief 用户连接数据
struct UserBaseData
{
public:
    using ptr = std::shared_ptr<UserBaseData>;
public:
    util::Socket        sock;        // 套接字文件描述符管理对象，以此来标记一个在线用户
    E_ServerSocketState state;       // 套接字状态

    uint8_t  xorCode{};    // 异或码，用于安全验证
    uint32_t appID{};      // 连接程序的ID

    UserBuffer send_buf;      // 要发送给用户的数据
    UserBuffer recv_buf;      // 接收用户发来的数据
    std::shared_ptr<char> temp_recvBuf;
    size_t package_len{}; // 发送给用户的数据包总长度

    time_t time_connect{}; // 用户连接被accept的时间
    time_t time_heart{};   // 用户上一次发送包的时间，该变量将在accept和解包时设置

    time_t time_shutdown{};
    std::atomic<bool> is_shutdown{};

public:
    UserBaseData();

    /// @brief 重置用户数据，并使其状态为Free
    void Reset();

    void Init(util::Socket sock);

    [[nodiscard]] inline bool isShutdown() const { return is_shutdown.load(); }

    /// @brief 是否已连接
    [[nodiscard]] inline bool isConnected() const { return state >= E_ServerSocketState::eConnected; }

    /// @brief 是否是安全连接
    [[nodiscard]] inline bool isSecure() const { return state >= E_ServerSocketState::eSecure; }

    /// @brief 是否已登录
    [[nodiscard]] inline bool isLoggedIn() const { return state >= E_ServerSocketState::eLoggedIn; }

    /// @brief 是否需要保存
    [[nodiscard]] inline bool isNeedSave() const { return state == E_ServerSocketState::eNeedSave; }

    [[nodiscard]] inline bool isGood() const { return state >= E_ServerSocketState::eConnected and !is_shutdown; }


    inline bool operator==(int fd) const { return sock == fd; }
    inline bool operator==(const UserBaseData &rhs) const { return sock == rhs.sock; }
    inline bool operator!=(int fd) const { return sock != fd; }
    inline bool operator!=(const UserBaseData &rhs) const { return sock != rhs.sock; }

};

// #pragma pack(pop, packing) // 恢复字节对齐状态

inline bool operator==(int fd, const UserBaseData &rhs) { return rhs == fd; }
inline bool operator!=(int fd, const UserBaseData &rhs) { return rhs != fd; }

}


#endif // !____USER_BASE_DATA_H
