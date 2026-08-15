#include "MessageHeader_Cmd.h"
#include "AppXmlConfig.h"
#include "log.h"
#include "CodecUtils.hpp"
#include "core_definations.h"
#include "Endian.h"
#include "msg_cmd.pb.h"
#include "NetBuffer.h"
#include "SFINAE.h"
#include <google/protobuf/message.h>

#include "LinearBuffer.h"

namespace yy::core {




namespace {



template<class T>
requires requires {
    requires std::is_standard_layout_v<T>;
    requires std::is_trivial_v<T>;
    requires !std::is_pointer_v<T>;
}
T XorBytes(T val, uint8_t xorCode) {
    auto p = reinterpret_cast<char*>(&val);
    for (int i = 0; i < sizeof(T); ++i) {
        *p ^= xorCode;
        ++p;
    }
    return val;
}

}








MessageHeader_Cmd::MessageHeader_Cmd(const google::protobuf::Message & message) {
    SetAllFieldsFromMessage(message);
}


MessageParseErrorCode MessageHeader_Cmd::ParseFromBuffer(net::NetBuffer &buf, uint8_t xorCode) {
    if(buf.GetDataSize() < kMinHeaderLen)
        return MessageParseErrorCode::eNotReceiveFullHeader;

    /*! 读入数据，边读边解密 !*/
    int peekedLen = 0;
    /**** CheckCode ****/
    buf.PeekToCBuffer(0, m_CheckCode.data(), sizeof m_CheckCode);
    m_CheckCode[0] ^= xorCode;
    m_CheckCode[1] ^= xorCode;
    if(not std::equal(m_CheckCode.begin(), m_CheckCode.end(), config::g_app_config->GetValue().check_code())) {
        return MessageParseErrorCode::eInvalidCheckCode;
    }
    peekedLen += sizeof(m_CheckCode);

    /**** TypeCmd ****/
    buf.PeekToPodStruct(peekedLen, m_TypeCmd);
    m_TypeCmd = XorNet_TypeCmd(xorCode);
    peekedLen += sizeof(m_TypeCmd);

    /**** BodyLength ****/
    buf.PeekToPodStruct(peekedLen, m_BodyLength);
    m_BodyLength = XorNet_BodyLength(xorCode);
    if(buf.GetDataSize() < kMinHeaderLen + m_BodyLength) {
        return MessageParseErrorCode::eNotReceiveFullLength;
    }
    peekedLen += sizeof(m_BodyLength);

    YLOG_TRACE("收到消息头<{}>：[{}][{}][{}]", kMinHeaderLen,
        std::string_view{m_CheckCode.data(), m_CheckCode.size()}, m_TypeCmd, m_BodyLength);

    //! Peek成功，移动Head
    buf.PopData(kMinHeaderLen);

    return MessageParseErrorCode::eNoError;
}

bool MessageHeader_Cmd::AppendIntoBuffer(util::LinearBuffer& buf, uint8_t xorCode) {
    if(buf.GetFreeSize() < kMinHeaderLen) {
        return false;
    }

    buf.AppendDataFromCBuffer(XorCheckCode(xorCode).data(), kCheckCodeSize) ;
    buf.AppendDataFromPODStruct(XorHost_TypeCmd(xorCode)) ;
    buf.AppendDataFromPODStruct(XorHost_BodyLength(xorCode)) ;

    return true;
}



void MessageHeader_Cmd::SetAllFieldsFromMessage(const google::protobuf::Message &message) {
    SetCheckCode(config::g_app_config->GetValue().check_code());

    const std::string fullName = message.GetDescriptor()->full_name();
    const auto it = g_name_to_cmd.find(fullName);
    const MessageCommand cmd = (it == g_name_to_cmd.end() ? MSG_Unknown : it->second);
    SetTypeCmd(static_cast<uint16_t>(cmd));

    SetBodyLength(message.ByteSizeLong());
}


std::array<char, MessageHeader_Cmd::kCheckCodeSize>
MessageHeader_Cmd::XorCheckCode(const uint8_t xorCode) const {
    std::array<char, kCheckCodeSize> result = m_CheckCode;
    result[0] ^= xorCode;
    result[1] ^= xorCode;
    return result;
}

uint32_t MessageHeader_Cmd::XorNet_BodyLength(const uint8_t xorCode) const { return net::network_to_host32(XorBytes(m_BodyLength, xorCode)); }

uint16_t MessageHeader_Cmd::XorNet_TypeCmd(const uint8_t xorCode) const { return net::network_to_host16(XorBytes(m_TypeCmd, xorCode)); }

uint32_t MessageHeader_Cmd::XorHost_BodyLength(const uint8_t xorCode) const { return XorBytes(net::host_to_network32(m_BodyLength), xorCode)  ; }

uint16_t MessageHeader_Cmd::XorHost_TypeCmd(const uint8_t xorCode) const { return XorBytes(net::host_to_network16(m_TypeCmd), xorCode); }

}
