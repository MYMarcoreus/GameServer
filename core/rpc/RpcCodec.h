#pragma once
#include <functional>
#include <google/protobuf/stubs/casts.h>

#include "net_definations.h"
#include "noncopyable.h"
#include "RpcHeader.h"

namespace yy::core::rpc
{
using RpcMessagePtr = std::shared_ptr<protocol::core::RpcMessage>;

class RpcCodec: public util::noncopyable {
    using F_ProtobufMessageDispatchCallback = std::function<void(const net::TcpConnectionPtr &, const RpcMessagePtr &)>;
    using F_ProtobufErrorMessageCallback = std::function<void(const net::TcpConnectionPtr &, yy::net::NetBuffer &, MessageParseErrorCode)>;

public:
    explicit RpcCodec(const F_ProtobufMessageDispatchCallback& msgCb, const F_ProtobufErrorMessageCallback& errCb = DefaultErrorCallback);

    ///@brief TcpConnection接收字节流到输入缓冲以后调用的回调函数，该函数用于处理字节流，解析并创建出消息，然后传递消息给ProtobufDispatcher
    void OnTcpData(const yy::net::TcpConnectionPtr &conn, yy::net::NetBuffer &buf);

    ///@brief 发送message（加Header后Send）
    void SendTCP(const yy::net::TcpConnectionPtr &conn, const yy::protocol::core::RpcMessage & message);

private:
    ///@brief 解析Buffer中的二进制数据，将其解析为protobuf的Message
    std::pair<RpcHeader, MessagePtr> Parse(const yy::net::TcpConnectionPtr& conn, yy::net::NetBuffer& buf,
                                               MessageParseErrorCode& outErrCode);

    static void DefaultErrorCallback(const yy::net::TcpConnectionPtr & conn, yy::net::NetBuffer & buf, MessageParseErrorCode);

    MessagePtr CreateRpcMessage();

    template<typename To, typename From>
    static To implicit_cast(From const &f) {
        return f;
    }

    template<typename To, typename From>
    std::shared_ptr<To> down_pointer_cast(const ::std::shared_ptr<From>& f) {
        if (false) {
            implicit_cast<From*, To*>(0);
        }
        return ::std::static_pointer_cast<To>(f);
    }

private:
    //! 通过由上层IServer子类设置为`ProtobufDispatcher<TcpConnectionPtr>::OnProtobufMessage`
    F_ProtobufMessageDispatchCallback   m_ProtobufMessageDispatchCallback;
    F_ProtobufErrorMessageCallback      m_ProtobufErrorMessageCallback;
    const ::google::protobuf::Message* m_prototype;

};

}

