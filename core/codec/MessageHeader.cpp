#include "MessageHeader.h"
#include "NetBuffer.h"
#include "core_definations.h"
#include "AppXmlConfig.h"
#include "log.h"
#include "Endian.h"
#include <algorithm>
#include <google/protobuf/message.h>
#include <array>

#include "SFINAE.h"

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








MessageHeader::MessageHeader(const google::protobuf::Message & message) {
    SetAllFieldsFromMessage(message);
}


MessageParseErrorCode MessageHeader::ParseFromBuffer(net::NetBuffer &buf, uint8_t xorCode) {
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

    /**** FullLength ****/
    buf.PeekToPodStruct(peekedLen, m_FullLength);
    m_FullLength = XorNetFullLength(xorCode);
    if(m_FullLength < kMinHeaderLen) {
        return MessageParseErrorCode::eInvalidFullLength;
    }
    if(buf.GetDataSize() < m_FullLength) {
        return MessageParseErrorCode::eNotReceiveFullLength;
    }
    peekedLen += sizeof(m_FullLength);

    /**** TypeNameLength ****/
    buf.PeekToPodStruct(peekedLen, m_TypeNameLength);
    m_TypeNameLength = XorNetNameLength(xorCode);
    if(buf.GetDataSize() < CalcHeaderLen()) { //! TypeName还没接收完全
        return MessageParseErrorCode::eNotReceiveFullHeader;
    }
    peekedLen += sizeof(m_TypeNameLength);

    /**** TypeName ****/
    if(m_TypeNameLength > 0) {
        buf.PeekToString(peekedLen, m_TypeName, m_TypeNameLength);
        m_TypeName = XorTypeName(xorCode);
    }
    peekedLen += m_TypeNameLength;

    YLOG_TRACE("收到消息头<{}>：[{}][{}][{}][{}]", CalcHeaderLen(),
               std::string_view{m_CheckCode.data(), m_CheckCode.size()},
               m_FullLength,
               m_TypeNameLength,
               m_TypeName);

    //! Peek成功，移动Head
    buf.PopData(CalcHeaderLen());

    return MessageParseErrorCode::eNoError;
}

bool MessageHeader::AppendIntoBuffer(util::SequentialBuffer& buf, uint8_t xorCode) {
    if(buf.GetFreeSize() < this->CalcHeaderLen()) {
        return false;
    }

    buf.AppendDataFromCBuffer(XorCheckCode(xorCode).data(), sizeof(m_CheckCode)) ;
    buf.AppendDataFromPODStruct(XorHostFullLength(xorCode)) ;
    buf.AppendDataFromPODStruct(XorHostNameLength(xorCode)) ;
    buf.AppendDataFromCBuffer(XorTypeName(xorCode).c_str(), m_TypeNameLength);

    return true;
}

uint8_t MessageHeader::GenerateXorCode() {
    std::mt19937  eng{std::random_device{}() }; // 真随机数
    static std::uniform_int_distribution<int> dis(1, 125); // [1, 125]
    uint8_t gen_val = static_cast<uint8_t>(dis(eng));

    return gen_val;
}

void MessageHeader::SetAllFieldsFromMessage(const google::protobuf::Message &message/*, const uint32_t cid*/) {
    SetCheckCode(config::g_app_config->GetValue().check_code());

    // SetClientID(cid);
    const std::string typeName = message.GetDescriptor()->full_name();
    SetTypeNameLength(typeName.length());
    SetTypeName(typeName);

    const int fullLen = kMinHeaderLen + typeName.length() + message.ByteSizeLong();
    SetFullLength(fullLen);
}


std::array<char, MessageHeader::kCheckCodeSize>
MessageHeader::XorCheckCode(const uint8_t xorCode) const {
    std::array<char, kCheckCodeSize> result = m_CheckCode;
    result[0] ^= xorCode;
    result[1] ^= xorCode;
    return result;
}

// uint32_t MessageHeader::XorClientID(const uint8_t xorCode) const { return m_ClientID ^ xorCode; }

uint32_t MessageHeader::XorNetFullLength(const uint8_t xorCode) const { return net::network_to_host32(XorBytes(m_FullLength, xorCode)); }

uint16_t MessageHeader::XorNetNameLength(const uint8_t xorCode) const { return net::network_to_host16(XorBytes(m_TypeNameLength, xorCode)); }

uint32_t MessageHeader::XorHostFullLength(const uint8_t xorCode) const { return XorBytes(net::host_to_network32(m_FullLength), xorCode)  ; }

uint16_t MessageHeader::XorHostNameLength(const uint8_t xorCode) const { return XorBytes(net::host_to_network16(m_TypeNameLength), xorCode); }

std::string MessageHeader::XorTypeName(uint8_t xorCode) const {
    std::string val = m_TypeName;
    std::ranges::for_each(val, [xorCode](char & ch) { ch ^= xorCode; });
    return val;
}

}
