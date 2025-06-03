#include "RingBuffer.h"
#include <google/protobuf/message.h>

namespace yy::util
{
bool RingBuffer::AppendDataFromProtobuf(const google::protobuf::Message& message)
{
    size_t message_size = message.ByteSizeLong();

    if (GetFreeSize() < message_size)
        return false;

    const size_t contiguous_space = m_Capacity - m_Tail;
    if (contiguous_space >= message_size)
    {
        // 直接序列化到环形缓冲区尾部连续空间
        if (!message.SerializeToArray(GetFreeBegin(), static_cast<int>(message_size)))
            return false;

        MoveTail(message_size);
        return true;
    }
    else
    {
        // 需要绕环，先临时缓冲序列化
        std::vector<uint8_t> temp_buf(message_size);
        if (!message.SerializeToArray(temp_buf.data(), static_cast<int>(message_size)))
            return false;

        // 调用已有接口写入（支持环绕写）
        return AppendDataFromCBuffer(temp_buf.data(), message_size);
    }
}

bool RingBuffer::PopDataToProtobuf(const std::shared_ptr<google::protobuf::Message> & outMsg, size_t message_size)
{
    if (!outMsg)
        return false;
    // if (message_size == 0)
    //     return true;

    if (GetDataSize() < message_size)
        return false;

    size_t contiguous_space = m_Capacity - m_Head; //!
    if (contiguous_space >= message_size)
    {
        // 缓冲区数据连续，直接反序列化
        if (!outMsg->ParseFromArray(GetDataBegin(), static_cast<int>(message_size)))
            return false;

        MoveHeadAndTryReset(message_size);
        return true;
    }
    else
    {
        // 数据环绕，先拷贝到临时缓冲区
        std::vector<uint8_t> temp_buf(message_size);
        if (!PopDataToCBuffer(temp_buf.data(), message_size))
            return false;

        // PopDataToCBuffer 已经调用 MoveHeadAndTryReset 了
        return outMsg->ParseFromArray(temp_buf.data(), static_cast<int>(message_size));
    }
}

std::string RingBuffer::PopDataAsString(size_t len)
{
    if (len == 0 || GetDataSize() < len)
        return {};

    std::string result(len, '\0'); // 预分配字符串空间，注意字符串内容是未初始化的

    if (!PopDataToCBuffer(result.data(), len))
        return {}; // 读取失败返回空字符串

    return result;
}


std::string RingBuffer::PopAllDataAsString()
{
    return PopDataAsString(GetDataSize());
}
}
