#include "ProtobufDispatcher.h"
#include <google/protobuf/message.h>

namespace yy::core {

using yy::net::TcpConnectionPtr;


void ProtobufDispatcher::OnProtobufMessage(const TcpConnectionPtr &conn, const MessagePtr &message) const {
    CallbackMap::const_iterator it = m_CallbacksMap.find(message->GetDescriptor());
    if (it != m_CallbacksMap.end()) {
        it->second->OnMessage(conn, message);
    }
    else {
        m_DefaultCallback(conn, message);
    }
}


}