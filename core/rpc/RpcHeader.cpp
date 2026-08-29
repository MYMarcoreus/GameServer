#include "RpcHeader.h"

#include "log.h"
#include "NetBuffer.h"
#include "rpc.pb.h"
#include "Endian.h"
#include "LinearBuffer.h"
#include "RemoteXmlConfig.h"

namespace yy::core::rpc
{
RpcHeader::RpcHeader(const protocol::core::RpcMessage  & rpcmsg):
    m_CheckCode{kCheckCode}, m_FullLength(kHeaderSize + rpcmsg.ByteSizeLong())
{}

MessageParseErrorCode RpcHeader::ParseFromBuffer(net::NetBuffer& buf)
{
    if(buf.GetDataSize() < kHeaderSize)
        return MessageParseErrorCode::eNotReceiveFullHeader;

    int peekedLen = 0;

    // 检查标识码
    buf.PeekToCBuffer(peekedLen, m_CheckCode.data(), kCheckCodeSize);
    if(not std::equal(m_CheckCode.begin(), m_CheckCode.end(), kCheckCode.begin())) {
        return MessageParseErrorCode::eInvalidCheckCode;
    }
    peekedLen += kCheckCodeSize;

    // 检查包总长度
    buf.PeekToPodStruct(peekedLen, m_FullLength);
    m_FullLength = net::network_to_host32(m_FullLength);
    if(m_FullLength < kHeaderSize) {
        return MessageParseErrorCode::eInvalidFullLength;
    }
    //! 校验RPC包总长度上限，防止恶意节点声明超大长度（OOM）
    if(m_FullLength > static_cast<uint32_t>(config::g_remote_config->GetValue().recvBytesMax)) {
        YLOG_ERROR("RPC消息总长度超限：{} > {}", m_FullLength, config::g_remote_config->GetValue().recvBytesMax)
        return MessageParseErrorCode::eInvalidFullLength;
    }
    if(buf.GetDataSize() < m_FullLength) {
        return MessageParseErrorCode::eNotReceiveFullLength;
    }
    peekedLen += sizeof(m_FullLength);

    YLOG_TRACE("收到消息头<{}>：[{}][{}]", kHeaderSize,
           std::string_view{m_CheckCode.data(), m_CheckCode.size()},
           m_FullLength)

    //! Peek成功，移动Head
    buf.PopData(kHeaderSize);

    return MessageParseErrorCode::eNoError;
}

bool RpcHeader::AppendIntoBuffer(util::LinearBuffer& buf)
{
    if(buf.GetFreeSize() < kHeaderSize) {
        return false;
    }

    buf.AppendDataFromCBuffer(m_CheckCode.data(), m_CheckCode.size());
    buf.AppendDataFromPODStruct(net::host_to_network32(m_FullLength));

    return true;
}
}
