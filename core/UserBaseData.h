#ifndef ____USER_BASE_DATA_H
#define ____USER_BASE_DATA_H

#include <cstring>
#include <atomic>
#include "TcpConnection.h"
#include "core_definations.h"
#include "net_definations.h"


namespace yy::core {

/// @brief 用户连接数据
class UserBaseData
{
public:
    using ptr = std::shared_ptr<UserBaseData>;

    enum class E_UserBaseState {
        eFree         = 0,
        eConnected    = 4,
        eSecure       = 5,
        eLoggedIn     = 6,
        eNeedSave     = 7,
    };

public:
    UserBaseData(net::TcpConnectionPtr conn, uint32_t appid);

    void SetState(E_UserBaseState state) { m_state = state; }

    /// @brief 是否已连接
    bool isConnected() const { return m_state != E_UserBaseState::eFree and m_state != E_UserBaseState::eNeedSave; }

    /// @brief 是否是安全连接
    bool isSecure() const { return m_state == E_UserBaseState::eSecure or m_state == E_UserBaseState::eLoggedIn; }

    /// @brief 是否已登录
    bool isLoggedIn() const { return m_state == E_UserBaseState::eLoggedIn; }

    /// @brief 是否需要保存
    bool isNeedSave() const { return m_state == E_UserBaseState::eNeedSave; }


private:
    E_UserBaseState       m_state;
    uint32_t              m_appID;   // 连接程序的ID
    net::TcpConnectionPtr m_conn;
};

// #pragma pack(pop, packing) // 恢复字节对齐状态


}


#endif // !____USER_BASE_DATA_H
