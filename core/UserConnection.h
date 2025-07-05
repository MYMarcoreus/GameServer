#ifndef ____USER_BASE_DATA_H
#define ____USER_BASE_DATA_H

#include <cstring>
#include <atomic>
#include "core_definations.h"
#include "net_definations.h"
#include "socket_definations.h"

namespace yy::net {
class TcpConnection;
}

namespace yy::core {



class ProtobufTcpCodec;
class ProtobufUdpCodec;

/// @brief 用户连接数据
class UserConnection
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

public:
    UserConnection(net::TcpConnectionPtr conn, ProtobufTcpCodec & tcpCodec, ProtobufUdpCodec & udpCodec);

    void Shutdown();

    void SendTCP(const MessagePtr & message) ;
    void SendTCP(const google::protobuf::Message & message);

    void SendUDP(const MessagePtr & message) ;
    void SendUDP(const google::protobuf::Message & message);

    void SetState(const E_UserBaseState state) { m_state = state; }

    void SetUID(const uint32_t uid) { m_uid = uid; }

    void BindUdp(net::UdpSessionPtr);

    ///Region GETTER
    net::TcpConnectionPtr GetConnection() { return m_tcpChannel; }

    /// @brief 是否已连接
    bool IsConnected() const { return m_state != E_UserBaseState::eFree and m_state != E_UserBaseState::eSavingData; }

    /// @brief 是否是安全连接
    bool IsSecure() const { return m_state == E_UserBaseState::eSecure or m_state == E_UserBaseState::eLoggedIn; }

    /// @brief 是否已登录
    bool IsLoggedIn() const { return m_state == E_UserBaseState::eLoggedIn; }

    /// @brief 是否需要保存
    bool IsNeedSave() const { return m_state == E_UserBaseState::eSavingData; }

    uint32_t GetUID() const { return m_uid; }
    uint64_t GetConnID() const;
    SocketApiWrapper::socket_t GetSocketFD() const;
    ///End GETTER

    net::TimerID RunAt(net::Timestamp time, net::F_TaskCallback cb);
    net::TimerID RunAfter(net::Microseconds delay, net::F_TaskCallback cb);
    net::TimerID RunEvery(net::Microseconds interval, net::F_TaskCallback cb);
    void CancelTimer(net::TimerID timerid);

private:
    E_UserBaseState                             m_state;
    uint32_t                                    m_uid;
    net::TcpConnectionPtr                       m_tcpChannel;
    net::UdpSessionPtr                          m_udpChannel;
    ProtobufTcpCodec &                          m_tcpCodec;
    ProtobufUdpCodec &                          m_udpCodec;
};

// #pragma pack(pop, packing) // 恢复字节对齐状态


}


#endif // !____USER_BASE_DATA_H

