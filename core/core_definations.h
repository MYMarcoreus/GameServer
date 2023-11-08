#ifndef LINUXGAMESERVER_CORE_DEFINATIONS_H
#define LINUXGAMESERVER_CORE_DEFINATIONS_H


#include <memory>


namespace google::protobuf {
class Message;
class Descriptor;
}

namespace yy::net {
class Socket;
}




namespace yy::core {


class UserBaseData;
using UserBaseDataPtr = std::shared_ptr<UserBaseData>;


enum class MessageParseErrorCode {
    eNoError = 0,
    eInvalidCheckCode = 1,
    eNotReceiveFullHeader = 2,
    eInvalidFullLength = 3,
    eNotReceiveFullLength = 5,
    eParseError = 6,
    eUnkonwnMessage = 7,
};

inline std::string ToString(MessageParseErrorCode code)
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


using MessagePtr = std::shared_ptr<google::protobuf::Message>;

}













#endif //LINUXGAMESERVER_CORE_DEFINATIONS_H
