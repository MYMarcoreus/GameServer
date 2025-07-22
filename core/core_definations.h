#ifndef LINUXGAMESERVER_CORE_DEFINATIONS_H
#define LINUXGAMESERVER_CORE_DEFINATIONS_H


#include <memory>
#include <string>

using std::shared_ptr;
using std::unique_ptr;
using std::make_unique;
using std::make_shared;

namespace google::protobuf {
class Descriptor;            // descriptor.h
class ServiceDescriptor;     // descriptor.h
class MethodDescriptor;      // descriptor.h
class Message;               // message.h
class Closure;
class RpcController;
class Service;
}


namespace yy::net {
class Socket;
class NetBuffer;
class EventLoop;
}

namespace yy::protocol::core {
class RpcMessage;
}

namespace yy::util {
class SequentialBuffer;
}



namespace yy::core {


class UserConnection;
using UserConnectionPtr = std::shared_ptr<UserConnection>;
using MessagePtr = std::shared_ptr<google::protobuf::Message>;

enum class MessageParseErrorCode {
    eNoError = 0,
    eInvalidCheckCode = 1,
    eNotReceiveFullHeader = 2,
    eInvalidFullLength = 3,
    eNotReceiveFullLength = 5,
    eParseError = 6,
    eUnkonwnMessage = 7,
};

inline std::string ToString(const MessageParseErrorCode code)
{
    switch (code) {
        case MessageParseErrorCode::eNoError:
            return "NoError";
        case MessageParseErrorCode::eInvalidCheckCode:
            return "InvalidCheckCode";
        case MessageParseErrorCode::eNotReceiveFullHeader:
            return "NotReceiveFullHeader";
        case MessageParseErrorCode::eInvalidFullLength:
            return "InvalidFullLength";
        case MessageParseErrorCode::eNotReceiveFullLength:
            return "NotReceiveFullLength";
        case MessageParseErrorCode::eParseError:
            return "ParseError";
        case MessageParseErrorCode::eUnkonwnMessage:
            return "UnkonwnMessage";
        default:
            return "UnknownError";
    }
}



enum class MessageType
{
    TCP = 0,
    UDP = 1
};

template<typename MsgT>
concept IsProtobufMessage = std::is_base_of_v<google::protobuf::Message, MsgT>;

}













#endif //LINUXGAMESERVER_CORE_DEFINATIONS_H

