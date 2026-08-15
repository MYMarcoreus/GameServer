#include "RpcCodec.h"
#include "rpc.pb.h"
#include "TcpConnection.h"
#include "NetBuffer.h"
#include "log.h"
#include <google/protobuf/message.h>
#include <string_view>

#include "RpcHeader.h"

namespace yy::core::rpc
{
using net::NetBuffer;
using net::TcpConnectionPtr;

RpcCodec::RpcCodec(const F_ProtobufMessageDispatchCallback& msgCb, const F_ProtobufErrorMessageCallback& errCb) :
    m_ProtobufMessageDispatchCallback{msgCb},
    m_ProtobufErrorMessageCallback{errCb},
    m_prototype{&protocol::core::RpcMessage::default_instance()}
{
    //pass
}

void RpcCodec::OnTcpData(const TcpConnectionPtr& conn, NetBuffer& buf)
{
    // 不断解析接收缓冲中的字节流，直到遇到不完整的信息或解析完毕
    while(buf.GetDataSize() >= RpcHeader::kHeaderSize)
    {
        MessageParseErrorCode errCode;

        //! 解析消息头，获得消息体
        auto [header, message] = Parse(conn, buf, errCode);
        bool isDone = false;
        switch (errCode) {
            //! 消息未接收完全
        case MessageParseErrorCode::eNotReceiveFullHeader:
        case MessageParseErrorCode::eNotReceiveFullLength:
            continue;
            //! 分发消息，交给其对应的处理函数处理
        case MessageParseErrorCode::eNoError:
            if(message)
                m_ProtobufMessageDispatchCallback(conn, down_pointer_cast<protocol::core::RpcMessage>(message));
            break;
        default:
            m_ProtobufErrorMessageCallback(conn, buf, errCode);
            isDone = true;
            break;
        }

        if(isDone)
            break;
    }
}

void RpcCodec::SendTCP(const TcpConnectionPtr& conn, const protocol::core::RpcMessage & message)
{
    //! 设置消息头
    RpcHeader header{message};

    /* 不用关心buffer空间不足，因为我们已经分配好了足够的空间 */
    //! 填充消息头
    util::LinearBuffer buffer{header.GetFullLength()+4};
    header.AppendIntoBuffer(buffer);

    YLOG_TRACE("发送消息头<{}>：[{}][{}]", header.kHeaderSize,
               std::string_view {header.GetCheckCode().data(), header.kCheckCodeSize},
               header.GetFullLength());

    //! 填充消息体
    buffer.AppendDataFromProtobuf(message);

    YLOG_TRACE("发送消息体<{}>", header.CalcBodyLen());

    //! 发送
    conn->SendRawTCP(std::string_view(buffer.Peek(), buffer.GetDataSize()));
}

std::pair<RpcHeader, MessagePtr> RpcCodec::Parse(const TcpConnectionPtr& conn, NetBuffer& buf, MessageParseErrorCode& outErrCode)
{
    RpcHeader header;

    //! 解析消息头
    outErrCode = header.ParseFromBuffer(buf);

    switch (outErrCode) {
        case MessageParseErrorCode::eNoError:
        case MessageParseErrorCode::eNotReceiveFullHeader:
        case MessageParseErrorCode::eNotReceiveFullLength: {
            break;
        }
        case MessageParseErrorCode::eInvalidCheckCode:
        case MessageParseErrorCode::eInvalidFullLength:
        case MessageParseErrorCode::eParseError:
        case MessageParseErrorCode::eUnkonwnMessage: {
            YLOG_ERROR("RpcCodec解析消息头失败<{}:{}>，{}", conn->GetSocketFD(), conn->GetConnID(), ToString(outErrCode).c_str())
            break;
        }
    }

    //! 解析消息体
    MessagePtr message = CreateRpcMessage();
    if(message) {
        const bool isOk = buf.PopDataToProtobuf(message, header.CalcBodyLen());
        //! 解析消息体失败！
        if(!isOk) {
            outErrCode = MessageParseErrorCode::eParseError;
        }

        YLOG_TRACE("解析消息体<{}>：", header.CalcBodyLen());

    } else {
        outErrCode = MessageParseErrorCode::eUnkonwnMessage;
    }

    return {header, message};
}

void RpcCodec::DefaultErrorCallback(const TcpConnectionPtr& conn, NetBuffer& buf, MessageParseErrorCode)
{
    if(conn and conn->IsConnected()) {
        conn->Shutdown();
    }
}

MessagePtr RpcCodec::CreateRpcMessage()
{
    MessagePtr message = nullptr;
    if (m_prototype) {
        message.reset(m_prototype->New());
    }
    return message;
}
}
