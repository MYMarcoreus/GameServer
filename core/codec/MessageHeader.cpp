#include "MessageHeader.h"
#include "Buffer.h"
#include "core_definations.h"
#include "AppXmlConfig.h"
#include "log.h"
#include <algorithm>

namespace yy::core {




namespace {



template<class T>
requires requires {
    requires std::is_standard_layout_v<T>;
    requires std::is_trivial_v<T>;
    requires !std::is_pointer_v<T>;
}
T CalcXor(T val, uint8_t xorCode) {
    char * p = (char *)&val;
    for (int i = 0; i < sizeof(T); ++i) {
        *p ^= xorCode;
        p++;
    }
    return val;
}

template<class T>
requires requires {
    requires yy::util::is_iterable_container_v<T>;
    requires std::is_standard_layout_v<typename T::value_type>;
    requires std::is_trivial_v<typename T::value_type>;
}
T CalcXor(T val, uint8_t xorCode) {
    std::for_each(std::begin(val), std::end(val), [xorCode](typename T::value_type & ch) { CalcXor(ch, xorCode); });
    return val;
}


}








MessageHeader::MessageHeader(const google::protobuf::Message & message) {
    SetAllFieldsFromMessage(message);
}


MessageParseErrorCode MessageHeader::RetrieveFromBuffer(net::Buffer &buf, uint8_t xorCode) {
    if(buf.GetDataSize() < kMinHeaderLen)
        return MessageParseErrorCode::eNotReceiveFullHeader;

    /*! 读入数据，边读边解密 !*/
    int peekedLen = 0;
    /**** CheckCode ****/
    buf.PeekCBuffer(0, m_CheckCode, sizeof m_CheckCode);
    strncpy(m_CheckCode, XorCheckCode(xorCode).c_str(), sizeof(m_CheckCode));
    if(strncmp(m_CheckCode, config::g_app_config->GetValue().check_code(), sizeof(m_CheckCode)) != 0) {
        return MessageParseErrorCode::eInvalidCheckCode;
    }
    peekedLen += sizeof(m_CheckCode);

    /**** FullLength ****/
    buf.PeekPodStruct(peekedLen, m_FullLength);
    m_FullLength = XorFullLength(xorCode);
    if(m_FullLength < kMinHeaderLen) {
        return MessageParseErrorCode::eInvalidFullLength;
    }
    if(buf.GetDataSize() < m_FullLength){
        return MessageParseErrorCode::eNotReceiveFullLength;
    }
    peekedLen += sizeof(m_FullLength);

    /**** TypeNameLength ****/
    buf.PeekPodStruct(peekedLen, m_TypeNameLength);
    m_TypeNameLength = XorNameLength(xorCode);
    if(buf.GetDataSize() < CalcHeaderLen()) { //! TypeName还没接收完全
        return MessageParseErrorCode::eNotReceiveFullHeader;
    }
    peekedLen += sizeof(m_TypeNameLength);

    /**** TypeName ****/
    if(m_TypeNameLength > 0) {
        m_TypeName.assign(buf.Peek(peekedLen), buf.Peek(peekedLen + m_TypeNameLength));
        m_TypeName = XorTypeName(xorCode);
    }
    peekedLen += m_TypeNameLength;


    YLOG_TRACE("收到消息头<{}>：[{}][{}][{}][{}]", CalcHeaderLen(), m_CheckCode, m_FullLength, m_TypeNameLength, m_TypeName);

    //! Peek成功，移动Head
    buf.MoveHead(CalcHeaderLen());

    return MessageParseErrorCode::eNoError;
}

bool MessageHeader::AppendIntoBuffer(net::Buffer &buf, uint8_t xorCode) {
    if(buf.GetFreeSize() < this->CalcHeaderLen()) {
        return false;
    }

    buf.AppendDataFromCBuffer(XorCheckCode(xorCode).c_str(), sizeof(m_CheckCode)) ;
    buf.AppendDataFromPODStruct(XorFullLength(xorCode)) ;
    buf.AppendDataFromPODStruct(XorNameLength(xorCode)) ;
    buf.AppendDataFromCBuffer(XorTypeName(xorCode).c_str(), m_TypeNameLength);

    return true;
}

uint8_t MessageHeader::GenerateXorCode() {
    std::mt19937  eng{std::random_device{}() }; // 真随机数
    static std::uniform_int_distribution<int> dis(1, 125); // [1, 125]
    uint8_t gen_val = static_cast<uint8_t>(dis(eng));

    return gen_val;
}

void MessageHeader::SetAllFieldsFromMessage(const google::protobuf::Message &message) {
    SetCheckCode(config::g_app_config->GetValue().check_code());

    std::string typeName = message.GetDescriptor()->full_name();
    SetTypeNameLength(typeName.length());
    SetTypeName(typeName);

    int fullLen = MessageHeader::kMinHeaderLen + typeName.length() + message.ByteSizeLong();
    SetFullLength(fullLen);
}


std::string MessageHeader::XorCheckCode(uint8_t xorCode) { std::string rst = m_CheckCode; rst[0]^=xorCode; rst[1]^=xorCode; return rst; }

uint32_t MessageHeader::XorFullLength(uint8_t xorCode) { return m_FullLength ^ xorCode; }

uint16_t MessageHeader::XorNameLength(uint8_t xorCode) { return m_TypeNameLength ^ xorCode; }

std::string MessageHeader::XorTypeName(uint8_t xorCode) {
    std::string val = m_TypeName;
    std::for_each(std::begin(val), std::end(val), [xorCode](char & ch) { ch ^= xorCode; });
    return val;
}




}
