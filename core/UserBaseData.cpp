#include "UserBaseData.h"
#include "ConfigManager.h"
#include "AppXmlConfig.h"
#include "log.h"
#include "codec/ProtobufCodec.h"
#include "EventLoop.h"

namespace yy::core {


UserBaseData::UserBaseData(net::TcpConnectionPtr conn, uint32_t appid, ProtobufCodec & m_codec)
        : m_conn{conn},
          m_state{E_UserBaseState::eConnected},
          m_appID(appid),
          m_uid{0},
          m_codec(m_codec)
{

}

void UserBaseData::Send(const MessagePtr &message) {
    if(message) {
        m_codec.Send(m_conn, *message);
    }
}

void UserBaseData::Send(const google::protobuf::Message &message) {
    m_codec.Send(m_conn, message);
}

net::TimerID UserBaseData::RunAt(net::Timestamp time, net::F_TimerCallback cb) {
    return m_conn->GetLoop()->RunAt(time, std::move(cb));
}

net::TimerID UserBaseData::RunAfter(net::Microseconds delay, net::F_TimerCallback cb) {
    return m_conn->GetLoop()->RunAfter(delay, std::move(cb));
}

net::TimerID UserBaseData::RunEvery(net::Microseconds interval, net::F_TimerCallback cb) {
    return m_conn->GetLoop()->RunEvery(interval, std::move(cb));
}

void UserBaseData::CancelTimer(net::TimerID timerid) {
    m_conn->GetLoop()->CancelTimer(timerid);
}


}
