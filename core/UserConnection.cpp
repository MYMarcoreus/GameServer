#include "UserConnection.h"
#include "ConfigManager.h"
#include "AppXmlConfig.h"
#include "log.h"
#include "codec/ProtobufTcpCodec.h"
#include "codec/ProtobufUdpCodec.h"
#include "EventLoop.h"

namespace yy::core {


UserConnection::UserConnection(net::TcpConnectionPtr conn, ProtobufTcpCodec & tcpCodec, ProtobufUdpCodec & udpCodec)
        : m_tcpChannel{conn},
          m_state{E_UserBaseState::eConnected},
          m_uid{0},
          m_tcpCodec(tcpCodec),
          m_udpCodec(udpCodec)
{

}

void UserConnection::SendTCP(const MessagePtr &message) {
    if(message) {
        m_tcpCodec.SendTCP(m_tcpChannel, *message);
    }
}

void UserConnection::SendTCP(const google::protobuf::Message &message) {
    m_tcpCodec.SendTCP(m_tcpChannel, message);
}

void UserConnection::SendUDP(const MessagePtr & message) {
    if(message) {
        m_udpCodec.SendUDP(m_udpChannel, *message);
    }
}

void UserConnection::SendUDP(const google::protobuf::Message &message) {
    m_udpCodec.SendUDP(m_udpChannel, message);
}


net::TimerID UserConnection::RunAt(net::Timestamp time, net::F_TaskCallback cb) {
    return m_tcpChannel->GetIOLoop()->RunAt(time, std::move(cb));
}

net::TimerID UserConnection::RunAfter(net::Microseconds delay, net::F_TaskCallback cb) {
    return m_tcpChannel->GetIOLoop()->RunAfter(delay, std::move(cb));
}

net::TimerID UserConnection::RunEvery(net::Microseconds interval, net::F_TaskCallback cb) {
    return m_tcpChannel->GetIOLoop()->RunEvery(interval, std::move(cb));
}

void UserConnection::CancelTimer(net::TimerID timerid) {
    m_tcpChannel->GetIOLoop()->CancelTimer(timerid);
}


}

