#include "UserConnection.h"
#include "ConfigManager.h"
#include "AppXmlConfig.h"
#include "log.h"
#include "codec/ProtobufCodec.h"
#include "EventLoop.h"

namespace yy::core {


UserConnection::UserConnection(net::TcpConnectionPtr conn, ProtobufCodec & m_codec)
        : m_conn{conn},
          m_state{E_UserBaseState::eConnected},
          m_uid{0},
          m_codec(m_codec)
{

}

void UserConnection::Send(const MessagePtr &message) {
    if(message) {
        m_codec.Send(m_conn, *message);
    }
}

void UserConnection::Send(const google::protobuf::Message &message) {
    m_codec.Send(m_conn, message);
}



net::TimerID UserConnection::RunAt(net::Timestamp time, net::F_TaskCallback cb) {
    return m_conn->GetLoop()->RunAt(time, std::move(cb));
}

net::TimerID UserConnection::RunAfter(net::Microseconds delay, net::F_TaskCallback cb) {
    return m_conn->GetLoop()->RunAfter(delay, std::move(cb));
}

net::TimerID UserConnection::RunEvery(net::Microseconds interval, net::F_TaskCallback cb) {
    return m_conn->GetLoop()->RunEvery(interval, std::move(cb));
}

void UserConnection::CancelTimer(net::TimerID timerid) {
    m_conn->GetLoop()->CancelTimer(timerid);
}


}

