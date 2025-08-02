#include "ProtobufTcpCodec_Name.h"
#include "NetBuffer.h"
#include "TcpConnection.h"
#include "AppXmlConfig.h"
#include "log.h"
#include "CodecUtils.hpp"
#include <google/protobuf/message.h>

namespace yy::core {

using net::NetBuffer;
using net::TcpConnectionPtr;



ProtobufTcpCodec_Name::ProtobufTcpCodec_Name(const F_ProtobufMessageDispatchCallback& msgCb,
                                   const F_ProtobufErrorMessageCallback& errCb)
    : m_ProtobufMessageDispatchCallback{msgCb},
      m_ProtobufErrorMessageCallback{errCb}
{ }


std::pair<MessageHeader_Name, MessagePtr> ProtobufTcpCodec_Name::Parse(const TcpConnectionPtr& conn, NetBuffer& buf, MessageParseErrorCode& outErrCode)
{
    MessageHeader_Name header;

    //! 解析消息头
    outErrCode = header.ParseFromBuffer(buf, conn->GetXorCode());
    if(outErrCode != MessageParseErrorCode::eNoError) {
        YLOG_ERROR("解析消息头失败<{}:{}>，{}", conn->GetSocketFD(), conn->GetConnID(), ToString(outErrCode).c_str())
    }

    //! 解析消息体
    MessagePtr message = CreateMessage(header.GetTypeName());
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



void ProtobufTcpCodec_Name::OnTcpData(const TcpConnectionPtr &conn, NetBuffer &buf) {
    // 不断解析接收缓冲中的字节流，直到遇到不完整的信息或解析完毕
    while(buf.GetDataSize() >= MessageHeader_Name::kMinHeaderLen)
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
                    m_ProtobufMessageDispatchCallback(conn, message);
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

void ProtobufTcpCodec_Name::SendTCP(const TcpConnectionPtr &conn, const google::protobuf::Message & message) {
    //! 设置消息头
    MessageHeader_Name header{message};

    /* 不用关心buffer空间不足，因为我们已经分配好了足够的空间 */
    //! 填充消息头
    const auto buffer = std::make_shared<util::SequentialBuffer>(header.GetFullLength()+4);
    header.AppendIntoBuffer(*buffer, conn->GetXorCode());

    //! 填充消息体
    buffer->AppendDataFromProtobuf(message);

    YLOG_TRACE("发送消息体<{}>", header.CalcBodyLen());

    //! 发送
    conn->SendRawTCP(buffer);
}


void ProtobufTcpCodec_Name::DefaultErrorCallback(const TcpConnectionPtr &conn, NetBuffer &buf, MessageParseErrorCode) {
    if(conn and conn->IsConnected()) {
        conn->Shutdown();
    }
}



}
