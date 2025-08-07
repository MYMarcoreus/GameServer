#pragma once

#include "core_definations.h"
#include <array>

namespace google::protobuf
{
class Message;
}

namespace yy::util {
class SequentialBuffer;
}

namespace yy::net {
class NetBuffer;
}

namespace yy::core {






// #pragma pack(push, packing) // 保存当前字节对齐状态
// #pragma pack(1) // 设为单字节对齐

/*************************** 游戏协议消息头 ***************************/
//! 消息头不是POD类型；保证消息头内的数据是未加密的、人类可读的。
class MessageHeader_Name
{
public:
    static constexpr int kCheckCodeSize = 2;

public:
    MessageHeader_Name() = default;

    /// @brief 发送时使用：用message初始化首部字段
    explicit MessageHeader_Name(const google::protobuf::Message & message);

    void SetCheckCode(const void * const value) {
        m_CheckCode[0] = ((char*)value)[0];
        m_CheckCode[1] = ((char*)value)[1];
    }

    /// @brief 接收时使用，从Buffer中读入未解密的数据，并解密
    MessageParseErrorCode ParseFromBuffer(net::NetBuffer &buf, uint8_t xorCode);

    /// @brief 将*this中的数据加密并序列化后写入Buffer中
    bool AppendIntoBuffer(util::SequentialBuffer& buf, uint8_t xorCode);

    const auto & GetCheckCode() const { return m_CheckCode; }

    // uint32_t GetClientID() const { return m_ClientID; }

    ///@brief 消息头+消息体长度
    uint32_t GetFullLength() const { return m_FullLength; }

    uint16_t GetTypeNameLength() const { return m_TypeNameLength; }

    ///@brief 消息类型名
    std::string GetTypeName() const { return m_TypeName; };

    ///@brief 消息头长度
    size_t CalcHeaderLen() const { return kMinHeaderLen + m_TypeNameLength; }

    ///@brief 消息体长度
    size_t CalcBodyLen() const { return m_FullLength - CalcHeaderLen(); }

private:
    ///@brief 设置首部结构的所有字段
    void SetAllFieldsFromMessage(const google::protobuf::Message &message/*, uint32_t cid*/);
    // void SetClientID    (const uint32_t     value) { m_ClientID = value; }
    void SetFullLength    (const uint32_t     value) { m_FullLength = value; }
    void SetTypeNameLength(const uint16_t     value) { m_TypeNameLength = value; }
    void SetTypeName(const std::string& typeName) { m_TypeName = typeName; }

    std::array<char, kCheckCodeSize> XorCheckCode(uint8_t xorCode) const;
    // uint32_t    XorClientID(uint8_t xorCode) const;
    uint32_t    XorNetFullLength(uint8_t xorCode) const;
    uint16_t    XorNetNameLength(uint8_t xorCode) const;

    uint32_t    XorHostFullLength(uint8_t xorCode) const;
    uint16_t    XorHostNameLength(uint8_t xorCode) const;

    std::string XorTypeName(uint8_t xorCode) const;
private:
    std::array<char, kCheckCodeSize> m_CheckCode;      // 2B: 用于验证该包是否是我们规定的游戏协议包
    // uint32_t                         m_ClientID;       // 4B：客户端ID
    uint32_t                         m_FullLength;     // 4B: 指示整个包的长度
    uint16_t                         m_TypeNameLength; // 2B: RPC类型
    std::string                      m_TypeName;       // TypeNameLength B：RPC名称
                                                       //... protobuf消息

public:
    static constexpr int kMinHeaderLen =
            sizeof(m_CheckCode) +
            // sizeof(MessageHeader_Name::m_ClientID) +
            sizeof(m_FullLength) +
            sizeof(m_TypeNameLength);
    static constexpr int kMaxHeaderLen = 128;
};
// #pragma pack(pop, packing) // 恢复字节对齐状态





}


