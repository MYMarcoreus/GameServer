#ifndef ____USER_BASE_DATA_H
#define ____USER_BASE_DATA_H

#include <cstring>
#include <atomic>
#include "TcpConnection.h"
#include "core_definations.h"
#include "net_definations.h"


namespace yy::core {


class ProtobufCodec;


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
    UserBaseData(net::TcpConnectionPtr conn, uint32_t appid, ProtobufCodec & m_codec);

    void Shutdown() { m_conn->Shutdown(); }

    void Send(const MessagePtr & message) ;
    void Send(const google::protobuf::Message & message);

    net::TcpConnectionPtr GetConnection() { return m_conn; }

    void SetState(E_UserBaseState state) { m_state = state; }

    void SetUID(uint32_t uid) { m_uid = uid; }

    /// @brief 是否已连接
    bool isConnected() const { return m_state != E_UserBaseState::eFree and m_state != E_UserBaseState::eNeedSave; }

    /// @brief 是否是安全连接
    bool isSecure() const { return m_state == E_UserBaseState::eSecure or m_state == E_UserBaseState::eLoggedIn; }

    /// @brief 是否已登录
    bool isLoggedIn() const { return m_state == E_UserBaseState::eLoggedIn; }

    /// @brief 是否需要保存
    bool isNeedSave() const { return m_state == E_UserBaseState::eNeedSave; }

    uint32_t GetUID() const { return m_uid; }

private:
    E_UserBaseState       m_state;
    uint32_t              m_appID;   // 连接程序的ID
    uint32_t              m_uid;
    net::TcpConnectionPtr m_conn;
    ProtobufCodec &       m_codec;
};

// #pragma pack(pop, packing) // 恢复字节对齐状态


}


#endif // !____USER_BASE_DATA_H
