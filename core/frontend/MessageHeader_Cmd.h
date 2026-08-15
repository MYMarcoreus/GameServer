#pragma once

#include "core_definations.h"
#include <array>

namespace google::protobuf
{
class Message;
}

namespace yy::util {
class LinearBuffer;
}

namespace yy::net {
class NetBuffer;
}

namespace yy::core {






// #pragma pack(push, packing) // 保存当前字节对齐状态
// #pragma pack(1) // 设为单字节对齐

/*************************** 通信协议消息头 ***************************/
class MessageHeader_Cmd
{
public:
    static constexpr int kCheckCodeSize = 2;

public:
    MessageHeader_Cmd() = default;

    /// @brief 发送时使用：用message初始化首部字段
    explicit MessageHeader_Cmd(const google::protobuf::Message & message);

    void SetCheckCode(const void * const value) {
        m_CheckCode[0] = ((char*)value)[0];
        m_CheckCode[1] = ((char*)value)[1];
    }

    /// @brief 接收时使用，从Buffer中读入未解密的数据，并解密
    MessageParseErrorCode ParseFromBuffer(net::NetBuffer &buf, uint8_t xorCode);

    /// @brief 将*this中的数据加密并序列化后写入Buffer中
    bool AppendIntoBuffer(util::LinearBuffer& buf, uint8_t xorCode);

    const auto& GetCheckCode() const { return m_CheckCode; }
    uint32_t    GetBodyLength() const { return m_BodyLength; }
    uint16_t    GetTypeCmd() const { return m_TypeCmd; }

private:
    ///@brief 设置首部结构的所有字段
    void SetAllFieldsFromMessage(const google::protobuf::Message &message);
    void SetTypeCmd(const uint16_t value) { m_TypeCmd = value; }
    void SetBodyLength(const uint32_t value) { m_BodyLength = value; }

    std::array<char, kCheckCodeSize> XorCheckCode(uint8_t xorCode) const;
    uint32_t    XorNet_BodyLength(uint8_t xorCode) const;
    uint16_t    XorNet_TypeCmd(uint8_t xorCode) const;
    uint32_t    XorHost_BodyLength(uint8_t xorCode) const;
    uint16_t    XorHost_TypeCmd(uint8_t xorCode) const;

private:
    std::array<char, kCheckCodeSize> m_CheckCode;       // 2B: 协议校验码
    uint16_t                         m_TypeCmd;         // 2B: 消息类型
    uint32_t                         m_BodyLength;      // 4B: 包体长度

public:
    static constexpr int kMinHeaderLen = kCheckCodeSize + sizeof(m_TypeCmd) + sizeof(m_BodyLength);
};
// #pragma pack(pop, packing) // 恢复字节对齐状态





}


