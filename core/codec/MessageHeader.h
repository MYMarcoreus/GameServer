#ifndef LINUXGAMESERVER_MESSAGEHEADER_H
#define LINUXGAMESERVER_MESSAGEHEADER_H


#include <cstdint>
#include <cstddef>
#include <random>
#include "ProtobufCodec.h"
#include "SFINAE.h"

namespace yy::net {
class Buffer;
}

namespace yy::core {






// #pragma pack(push, packing) // 保存当前字节对齐状态
// #pragma pack(1) // 设为单字节对齐

/*************************** 游戏协议消息头 ***************************/
//! 消息头不是POD类型；保证消息头内的数据是未加密的、人类可读的。
class MessageHeader
{
public:
    MessageHeader() = default;

    //! 发送时使用：用message初始化首部字段
    MessageHeader(const google::protobuf::Message & message);

    void SetCheckCode(const void * const value) {
        m_CheckCode[0] = ((char*)value)[0];
        m_CheckCode[1] = ((char*)value)[1];
    }
    void SetFullLength    (uint32_t     value) { m_FullLength = value; }
    void SetTypeNameLength(uint16_t     value) { m_TypeNameLength = value; }
    void SetTypeName(std::string typeName) { m_TypeName = typeName; }

    ///@brief 设置首部结构的所有字段
    void SetAllFieldsFromMessage(const google::protobuf::Message &message);

    std::string XorCheckCode(uint8_t xorCode);
    uint32_t    XorFullLength(uint8_t xorCode);
    uint16_t    XorNameLength(uint8_t xorCode);;
    std::string XorTypeName(uint8_t xorCode);;

    /// @brief 从Buffer中读入未解密的数据，并解密
    MessageParseErrorCode RetrieveFromBuffer(net::Buffer &buf, uint8_t xorCode);

    /// @brief 将*this中的数据加密并序列化后写入Buffer中
    bool AppendIntoBuffer(net::Buffer &buf, uint8_t xorCode);



    std::string GetCheckCode() const { return m_CheckCode; }

    ///@brief 消息头+消息体长度
    size_t GetFullLength() const { return m_FullLength; }

    uint16_t GetTypeNameLength() const { return m_TypeNameLength; }

    ///@brief 消息类型名
    std::string GetTypeName() const { return m_TypeName; };



    ///@brief 消息头长度
    size_t CalcHeaderLen() const { return kMinHeaderLen + m_TypeNameLength; }

    ///@brief 消息体长度
    size_t CalcBodyLen() const { return m_FullLength - CalcHeaderLen(); }



    // 随机产生一个异或码
    static uint8_t GenerateXorCode();

private:
    char        m_CheckCode[2];   // 2B: 用于验证该包是否是我们规定的游戏协议包
    uint32_t    m_FullLength;     // 4B: 指示整个包的长度
    uint16_t    m_TypeNameLength; // 2B: RPC类型
    std::string m_TypeName;       // TypeNameLength B：RPC名称
                                  //... protobuf消息

public:
    static constexpr int kMinHeaderLen =
            sizeof(MessageHeader::m_CheckCode) +
            sizeof(MessageHeader::m_FullLength) +
            sizeof(MessageHeader::m_TypeNameLength);
    static constexpr int kMaxHeaderLen = 128;
};
// #pragma pack(pop, packing) // 恢复字节对齐状态





}


#endif //LINUXGAMESERVER_MESSAGEHEADER_H

