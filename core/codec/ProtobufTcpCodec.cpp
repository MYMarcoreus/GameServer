#include "ProtobufTcpCodec.h"
#include "Buffer.h"
#include "TcpConnection.h"
#include "AppXmlConfig.h"
#include "log.h"
#include <google/protobuf/message.h>
#include <google/protobuf/message_lite.h>

namespace yy::core {

using ::yy::net::Buffer;
using ::yy::net::TcpConnectionPtr;



ProtobufTcpCodec::ProtobufTcpCodec(ProtobufTcpCodec::F_ProtobufMessageDispatchCallback  msgCb,
                                   ProtobufTcpCodec::F_ProtobufErrorMessageCallback     errCb)
    : m_ProtobufMessageDispatchCallback{msgCb},
      m_ProtobufErrorMessageCallback{errCb}
{ }


MessagePtr ProtobufTcpCodec::Parse(const TcpConnectionPtr &conn, Buffer &buf, MessageParseErrorCode & outErrCode) {
    MessageHeader header;

    //! 解析消息头
    outErrCode = header.ParseFromBuffer(buf, conn->GetXorCode());

    MessagePtr message{};
    //! 解析消息头成功
    if(outErrCode == MessageParseErrorCode::eNoError) {
        //! 解析消息体
        message = CreateMessage(header.GetTypeName());
        if(message) {
            bool isOk = buf.PopDataToProtobuf(message, header.CalcBodyLen());
            //! 解析消息体失败！
            if(!isOk) {
                outErrCode = MessageParseErrorCode::eParseError;
            }

            YLOG_TRACE("解析消息体<{}>：", header.CalcBodyLen());

        } else {
            outErrCode = MessageParseErrorCode::eUnkonwnMessage;
        }
    } else {
        YLOG_ERROR("解析消息头失败<{}:{}>，{}", conn->GetSocketFD(), conn->GetName().c_str(), ToString(outErrCode).c_str())
    }

    return message;
}



void ProtobufTcpCodec::OnData(const TcpConnectionPtr &conn, Buffer &buf) {
    // 不断解析接收缓冲中的字节流，直到遇到不完整的信息或解析完毕
    while(buf.GetDataSize() >= MessageHeader::kMinHeaderLen)
    {
        MessageParseErrorCode errCode;

        //! 解析消息头，获得消息体
        MessagePtr message = Parse(conn, buf, errCode);
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

void ProtobufTcpCodec::SendTCP(const TcpConnectionPtr &conn, const google::protobuf::Message & message) {
    //! 设置消息头
    MessageHeader header{message};

    /* 不用关心buffer空间不足，因为我们已经分配好了足够的空间 */
    //! 填充消息头
    Buffer buffer{header.GetFullLength()+4};
    header.AppendIntoBuffer(buffer, conn->GetXorCode());

    YLOG_TRACE("发送消息头<{}>：[{}][{}][{}][{}]", header.CalcHeaderLen(),
               header.GetCheckCode(), header.GetFullLength(), header.GetTypeNameLength(), header.GetTypeName());

    //! 填充消息体
    buffer.AppendDataFromProtobuf(message);

    YLOG_TRACE("发送消息体<{}>", header.CalcBodyLen());

    //! 发送
    conn->SendTCP(buffer);
}



void ProtobufTcpCodec::DefaultErrorCallback(const TcpConnectionPtr &conn, Buffer &buf, MessageParseErrorCode) {
    if(conn and conn->IsConnected()) {
        conn->Shutdown();
    }
}

MessagePtr ProtobufTcpCodec::CreateMessage(const std::string &typeName) {
    using google::protobuf::Message;
    using google::protobuf::Descriptor;
    using google::protobuf::DescriptorPool;
    using google::protobuf::MessageFactory;

    MessagePtr message = nullptr;
    const Descriptor * des = DescriptorPool::generated_pool()->FindMessageTypeByName(typeName);
    if(des)
    {
        const Message * prototype = MessageFactory::generated_factory()->GetPrototype(des);
        if(prototype) {
            message.reset(prototype->New());
        }
    }
    return message;
}



}
