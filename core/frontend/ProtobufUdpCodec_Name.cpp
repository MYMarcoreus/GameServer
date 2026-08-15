#include "ProtobufUdpCodec_Name.h"
#include "NetBuffer.h"
#include "TcpConnection.h"
#include "log.h"
#include "UdpSession.h"
#include "MessageHeader_Name.h"
#include "Socket.h"
#include "CodecUtils.hpp"
#include <google/protobuf/message.h>

namespace yy::core {

using net::NetBuffer;
using net::UdpSessionPtr;



ProtobufUdpCodec_Name::ProtobufUdpCodec_Name(F_ProtobufMessageDispatchCallback  msgCb,
                                   F_ProtobufErrorMessageCallback     errCb)
    : m_ProtobufMessageDispatchCallback{std::move(msgCb)},
      m_ProtobufErrorMessageCallback{std::move(errCb)}
{ }


std::pair<MessageHeader_Name, MessagePtr> ProtobufUdpCodec_Name::Parse(const UdpSessionPtr& udpSession, NetBuffer& buf, MessageParseErrorCode& outErrCode)
{
    MessageHeader_Name header;

    //! 解析消息头
    outErrCode = header.ParseFromBuffer(buf, udpSession->GetXorCode());

    MessagePtr message{};
    //! 解析消息头成功
    if(outErrCode == MessageParseErrorCode::eNoError) {
        //! 解析消息体
        message = CreateMessage(header.GetTypeName());
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
    } else {
        YLOG_ERROR("解析消息头失败<{}>，{}", udpSession->GetConnID(), ToString(outErrCode).c_str())
    }

    return {header, message};
}



void ProtobufUdpCodec_Name::OnData(const UdpSessionPtr & udpSession, NetBuffer & buf) {
    // 不断解析接收缓冲中的字节流，直到遇到不完整的信息或解析完毕
    while(buf.GetDataSize() >= MessageHeader_Name::kMinHeaderLen)
    {
        MessageParseErrorCode errCode;

        //! 解析消息头，获得消息体
        auto [header, message] = Parse(udpSession, buf, errCode);
        bool isDone = false;
        switch (errCode) {
            //! 消息未接收完全
            case MessageParseErrorCode::eNotReceiveFullHeader:
            case MessageParseErrorCode::eNotReceiveFullLength:
                continue;
            //! 分发消息，交给其对应的处理函数处理
            case MessageParseErrorCode::eNoError:
                if(message) {
                    YLOG_TRACE("收到Udp消息: {}", message->GetTypeName())
                    m_ProtobufMessageDispatchCallback(udpSession, message);
                }
                break;
            default:
                m_ProtobufErrorMessageCallback(udpSession, buf, errCode);
                isDone = true;
                break;
        }

        if(isDone)
            break;
    }
}

void ProtobufUdpCodec_Name::SendUDP(const UdpSessionPtr &udpSession, const google::protobuf::Message & message) {
    if(udpSession == nullptr)
        return;

    //! 设置消息头
    MessageHeader_Name header{message};

    /* 不用关心buffer空间不足，因为我们已经分配好了足够的空间 */
    //! 填充消息头
    const auto buffer = std::make_shared<util::LinearBuffer>(header.GetFullLength()+4);
    header.AppendIntoBuffer(*buffer, udpSession->GetXorCode());

    //! 填充消息体
    buffer->AppendDataFromProtobuf(message);

    YLOG_TRACE("发送消息体<{}>", header.CalcBodyLen());

    //! 发送
    udpSession->SendUDP(buffer);
}



void ProtobufUdpCodec_Name::DefaultErrorCallback(const UdpSessionPtr &udpSession, NetBuffer &buf, MessageParseErrorCode) {

}



}
