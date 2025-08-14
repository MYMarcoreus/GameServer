#ifndef LINUXGAMESERVER_CORE_DEFINATIONS_H
#define LINUXGAMESERVER_CORE_DEFINATIONS_H


#include <memory>
#include <string>

namespace google::protobuf {
class Descriptor;            // descriptor.h
class ServiceDescriptor;     // descriptor.h
class MethodDescriptor;      // descriptor.h
class Message;               // message.h
class Closure;
class RpcController;
class Service;
class RpcChannel;
}


namespace yy::net {
class Socket;
class NetBuffer;
class EventLoop;
class ThreadPool;
}

namespace yy::protocol::core {
class RpcMessage;
}

namespace yy::util {
class LinearBuffer;
}



namespace yy::core {

namespace rpc
{
class RpcCodec;
using RpcMessagePtr = std::shared_ptr<protocol::core::RpcMessage>;

}

using UID_t = uint64_t;

class IServer;
class UserConnection;
using UserConnectionPtr = std::shared_ptr<UserConnection>;
using MessagePtr = std::shared_ptr<google::protobuf::Message>;

// 首部用Protobuf消息的类型名字符串标识消息
class ProtobufTcpCodec_Name;
class ProtobufUdpCodec_Name;
// 首部用数字表示消息类型
class ProtobufTcpCodec_Cmd;
class ProtobufUdpCodec_Cmd;

using ProtobufTcpCodec = ProtobufTcpCodec_Cmd;
using ProtobufUdpCodec = ProtobufUdpCodec_Cmd;


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



enum class MessageNetType
{
    TCP = 0,
    UDP = 1
};

template<typename MsgT>
concept IsProtobufMessage = std::is_base_of_v<google::protobuf::Message, MsgT>;

template<typename ClassT, typename MsgT>
concept MessageHandlerInvocable = requires(
    ClassT* self,
    const UserConnectionPtr& user,
    const std::shared_ptr<MsgT>& msg,
    void (ClassT::*handler)(const UserConnectionPtr&, const std::shared_ptr<MsgT>&))
{
    (self->*handler)(user, msg);
};

template <typename T>
concept HasToken = requires(T t) {
    { t.token() } -> std::convertible_to<std::string>;
};

template <typename T>
concept HasUserToken = requires(T t) {
    { t.user_token() } -> std::convertible_to<std::string>;
};

template <typename T>
concept HasTokenMethod = HasToken<T> or HasUserToken<T>;

}













#endif //LINUXGAMESERVER_CORE_DEFINATIONS_H

