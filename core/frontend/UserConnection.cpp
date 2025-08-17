#include "UserConnection.h"
#include "ConfigManager.h"
#include "log.h"
#include "ProtobufTcpCodec_Cmd.h"
#include "ProtobufUdpCodec_Cmd.h"
#include "EventLoop.h"
#include "UdpSession.h"
#include "TcpConnection.h"

using namespace yy::net;

namespace yy::core {


UserConnection::UserConnection(const TcpConnectionPtr& conn, ProtobufTcpCodec & tcpCodec, ProtobufUdpCodec & udpCodec)
        : m_state{E_UserBaseState::eConnected},
          m_uid{0},
          m_tcpChannel{conn},
          m_tcpCodec(tcpCodec),
          m_udpCodec(udpCodec)
{
    UpdateHeartTime();
}

void UserConnection::Shutdown()
{
    m_tcpChannel->Shutdown();
    m_state.store(E_UserBaseState::eFree);
}

uint64_t UserConnection::GetConnID() const { return m_tcpChannel->GetConnID(); }


void UserConnection::SendTCP(const MessagePtr &message) const
{
    if(message) {
        m_tcpCodec.SendTCP(m_tcpChannel, *message);
    }
}

void UserConnection::SendTCP(const google::protobuf::Message &message) const
{
    m_tcpCodec.SendTCP(m_tcpChannel, message);
}

void UserConnection::SendUDP(const MessagePtr & message) {
    if(message and m_udpChannel) {
        m_udpCodec.SendUDP(m_udpChannel, *message);
    }
}

void UserConnection::SendUDP(const google::protobuf::Message &message) const
{
    if(m_udpChannel) {
        m_udpCodec.SendUDP(m_udpChannel, message);
    }
}


TimerID UserConnection::RunAt(const Timestamp time, F_TaskCallback cb) const
{
    return m_tcpChannel->GetIOLoop()->RunAt(time, std::move(cb));
}

TimerID UserConnection::RunAfter(const Microseconds delay, F_TaskCallback cb) const
{
    return m_tcpChannel->GetIOLoop()->RunAfter(delay, std::move(cb));
}

TimerID UserConnection::RunEvery(const Microseconds interval, F_TaskCallback cb) const
{
    return m_tcpChannel->GetIOLoop()->RunEvery(interval, std::move(cb));
}

void UserConnection::CancelTimer(const TimerID timerid) const
{
    m_tcpChannel->GetIOLoop()->CancelTimer(timerid);
}

void UserConnection::BindUdp(const UdpSessionPtr& u) {
    m_udpChannel = u;
}

}

