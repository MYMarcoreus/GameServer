#pragma once

#include <atomic>
#include "core_definations.h"
#include "net_definations.h"
#include "noncopyable.h"
#include "Timestamp.h"

namespace yy::core {



/// @brief 用户连接数据
class UserConnection: util::noncopyable
{
public:

    enum class E_UserBaseState {
        eFree         = 0,
        eConnected    = 4,
        eSecure       = 5,
        //todo 这两个状态实际上是业务层的，应该再业务层创建一个类来包含UserConnection
        eLoggedIn     = 6,
        eSavingData   = 7,
    };

    UserConnection(const net::TcpConnectionPtr& conn, ProtobufTcpCodec & tcpCodec, ProtobufUdpCodec & udpCodec);

    void BindUdp(const net::UdpSessionPtr&);

    void Shutdown();

    void SendTCP(const MessagePtr & message) const;
    void SendTCP(const google::protobuf::Message & message) const;
    void SendUDP(const MessagePtr & message) ;
    void SendUDP(const google::protobuf::Message & message) const;

    //Region SETTER
    void SetState(const E_UserBaseState state) { m_state.store(state, std::memory_order::release); }
    void SetUID(const uint32_t uid) { m_uid = uid; }
    void SetToken(const std::string & token) { m_token = token; }
    //End


    //Region GETTER
    net::TcpConnectionPtr GetConnection() { return m_tcpChannel; }

    /// @brief 是否已连接
    bool IsConnected() const { return m_state.load(std::memory_order::acquire) != E_UserBaseState::eFree and
                                      m_state.load(std::memory_order::acquire) != E_UserBaseState::eSavingData; }

    /// @brief 是否是安全连接
    bool IsSecure() const { return m_state.load(std::memory_order::acquire) == E_UserBaseState::eSecure or
                                   m_state.load(std::memory_order::acquire) == E_UserBaseState::eLoggedIn; }

    /// @brief 是否已登录
    bool IsLoggedIn() const { return m_state.load(std::memory_order::acquire) == E_UserBaseState::eLoggedIn; }

    /// @brief 是否需要保存
    bool IsNeedSave() const { return m_state.load(std::memory_order::acquire) == E_UserBaseState::eSavingData; }

    void UpdateHeartTime() { return m_heartTime.SetNow(); }
    auto GetHeartTime() const -> net::Timestamp { return  m_heartTime; }

    uint64_t    GetUID() const { return m_uid; }

    uint64_t    GetConnID() const;

    std::string GetToken() const { return m_token; }
    //End GETTER

    //Region 定时器相关
    net::TimerID RunAt(net::Timestamp time, net::F_TaskCallback cb) const;
    net::TimerID RunAfter(net::Microseconds delay, net::F_TaskCallback cb) const;
    net::TimerID RunEvery(net::Microseconds interval, net::F_TaskCallback cb) const;
    void CancelTimer(net::TimerID timerid) const;
    //End

private:
    std::atomic<E_UserBaseState>                m_state;
    uint64_t                                    m_uid;
    std::string                                 m_token;
    net::TcpConnectionPtr                       m_tcpChannel;
    net::UdpSessionPtr                          m_udpChannel;
    ProtobufTcpCodec &                          m_tcpCodec;
    ProtobufUdpCodec &                          m_udpCodec;
    net::Timestamp                              m_heartTime;
};

// #pragma pack(pop, packing) // 恢复字节对齐状态


}
