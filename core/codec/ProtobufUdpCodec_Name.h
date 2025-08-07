#pragma once

#include "noncopyable.h"
#include "core_definations.h"
#include "MessageHeader_Name.h"
#include "net_definations.h"

namespace google::protobuf {
class Message;
}

namespace yy::net {
class NetBuffer;
}

namespace yy::core {

/*
Message回调的传递路线(SetCallback)：
                                    调用MessageCallback(NetBuffer&)
                        ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
                        ▼                                             ┃
                  ProtobufCodec ==> XXXServer ==> TcpServer ==> TcpConnection
                        ┃
                        ┃                     注册回调(std::shared_ptr<T>&，T是Message的子类)
                        ┃                         ┏━━━━━━━━━━━━━━━━━━━┓
                        ▼                         ▼                   ┃
                  ProtobufCodec <== ProtobufDispatcher <== AAAServer、BBBServer、CCCServer
                        ┃                   ▲     ┃                   ▲
                        ┗━━━━━━━━━━━━━━━━━━━┛     ┗━━━━━━━━━━━━━━━━━━━┛
                     调用Message回调(MessagePtr&)  调用Message回调(MessagePtr&)

Message的传递路线(Call Callback)：
                  TcpConnection(将字节流读取到输入缓冲中，然后调用ProtobufCodec的回调)
                  ==> ProtobufCodec(解析输入缓冲中的字节流，将字节流分为一个一个的信息，然后调用ProtobufDispatcher的Dispatcher回调)
                  ==> ProtobufDispatcher(根据信息的种类调用在XXXServer中注册的回调)
                  ==> AAAServer、BBBServer、CCCServer(注册回调)
*/
class ProtobufUdpCodec_Name: public util::noncopyable {
    using F_ProtobufMessageDispatchCallback = std::function<void(const net::UdpSessionPtr &, const MessagePtr &)>;
    using F_ProtobufErrorMessageCallback = std::function<void(const net::UdpSessionPtr &, net::NetBuffer &, MessageParseErrorCode)>;

public:
    explicit ProtobufUdpCodec_Name(F_ProtobufMessageDispatchCallback msgCb, F_ProtobufErrorMessageCallback errCb = DefaultErrorCallback);

    ///@brief TcpConnection接收字节流到输入缓冲以后调用的回调函数，该函数用于处理字节流，解析并创建出消息，然后传递消息给ProtobufDispatcher
    void OnData(const net::UdpSessionPtr & udpSession, net::NetBuffer &buf);

    ///@brief 发送message（加Header后Send）
    void SendUDP(const net::UdpSessionPtr &udpSession, const google::protobuf::Message & message);

private:
    ///@brief 解析Buffer中的二进制数据，将其解析为protobuf的Message
    std::pair<MessageHeader_Name, MessagePtr> Parse(const net::UdpSessionPtr& udpSession, net::NetBuffer& buf, MessageParseErrorCode& outErrCode);

    static void DefaultErrorCallback(const net::UdpSessionPtr & udpSession, net::NetBuffer & buf, MessageParseErrorCode);

private:
    //! 通过由上层IServer子类设置为`ProtobufDispatcher<UdpSessionPtr>::OnProtobufMessage`
    F_ProtobufMessageDispatchCallback   m_ProtobufMessageDispatchCallback;
    F_ProtobufErrorMessageCallback      m_ProtobufErrorMessageCallback;
};

}
