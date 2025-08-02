#pragma once

#include <cstring>
#include <atomic>
#include "core_definations.h"
#include "net_definations.h"
#include "socket_definations.h"

using namespace yy::net;

namespace yy::core {



class ProtobufTcpCodec_Name;
class ProtobufUdpCodec_Name;

/// @brief 用户连接数据
class UserConnection: util::noncopyable
{
public:
    using ptr = std::shared_ptr<UserConnection>;

    enum class E_UserBaseState {
        eFree         = 0,
        eConnected    = 4,
        eSecure       = 5,
        //todo 这两个状态实际上是业务层的，应该再业务层创建一个类来包含UserConnection
        eLoggedIn     = 6,
        eSavingData   = 7,
    };

    UserConnection(const TcpConnectionPtr& conn, ProtobufTcpCodec & tcpCodec, ProtobufUdpCodec & udpCodec);

    void BindUdp(const UdpSessionPtr&);

    void Shutdown() const;

    void SendTCP(const MessagePtr & message) const;
    void SendTCP(const google::protobuf::Message & message) const;
    void SendUDP(const MessagePtr & message) ;
    void SendUDP(const google::protobuf::Message & message) const;

    //Region SETTER
    void SetState(const E_UserBaseState state) { m_state = state; }
    void SetUID(const uint32_t uid) { m_uid = uid; }
    void SetToken(const std::string & token) { m_token = token; }
    //End


    //Region GETTER
    TcpConnectionPtr GetConnection() { return m_tcpChannel; }

    /// @brief 是否已连接
    bool IsConnected() const { return m_state != E_UserBaseState::eFree and m_state != E_UserBaseState::eSavingData; }

    /// @brief 是否是安全连接
    bool IsSecure() const { return m_state == E_UserBaseState::eSecure or m_state == E_UserBaseState::eLoggedIn; }

    /// @brief 是否已登录
    bool IsLoggedIn() const { return m_state == E_UserBaseState::eLoggedIn; }

    /// @brief 是否需要保存
    bool IsNeedSave() const { return m_state == E_UserBaseState::eSavingData; }

    uint32_t    GetUID() const { return m_uid; }

    uint64_t    GetConnID() const;

    std::string GetToken() const { return m_token; }

    SocketApiWrapper::socket_t GetSocketFD() const;
    //End GETTER

    //Region 定时器相关
    TimerID RunAt(Timestamp time, F_TaskCallback cb) const;
    TimerID RunAfter(Microseconds delay, F_TaskCallback cb) const;
    TimerID RunEvery(Microseconds interval, F_TaskCallback cb) const;
    void CancelTimer(TimerID timerid) const;
    //End

private:
    E_UserBaseState                             m_state;
    uint64_t                                    m_uid;
    std::string                                 m_token;
    TcpConnectionPtr                            m_tcpChannel;
    UdpSessionPtr                               m_udpChannel;
    ProtobufTcpCodec &                          m_tcpCodec;
    ProtobufUdpCodec &                          m_udpCodec;
};

// #pragma pack(pop, packing) // 恢复字节对齐状态


}
