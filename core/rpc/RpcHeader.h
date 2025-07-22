#pragma once
#include <array>
#include "core_definations.h"

namespace yy::core::rpc
{

class RpcHeader {
public:
    static constexpr int kCheckCodeSize = 4;
    static constexpr std::array<char, kCheckCodeSize> kCheckCode = {'R', 'P', 'C', '*'};
    static constexpr int kHeaderSize =  kCheckCodeSize + sizeof(uint32_t);

    RpcHeader() = default;
    explicit RpcHeader(const protocol::core::RpcMessage & rpcmsg);

    MessageParseErrorCode ParseFromBuffer(net::NetBuffer &buf);

    bool AppendIntoBuffer(util::SequentialBuffer& buf);

    ///@brief 协议校验码
    const auto & GetCheckCode() const { return m_CheckCode; }

    ///@brief 消息头+消息体长度
    uint32_t GetFullLength() const { return m_FullLength; }

    size_t CalcBodyLen() const { return m_FullLength - kHeaderSize; }

private:
    std::array<char, kCheckCodeSize> m_CheckCode;      // 4B: 用于验证该包是否是我们规定的RPC协议包
    uint32_t                         m_FullLength;     // 4B: 指示整个包的长度
};

}
