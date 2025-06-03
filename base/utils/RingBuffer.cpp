#include "RingBuffer.h"
#include <google/protobuf/message.h>
#include <algorithm>

namespace yy::util
{
bool RingBuffer::PeekToCBuffer(int start_index, void* dest, size_t need_len) const
{
    if (start_index < 0)
        return false;
    if (need_len == 0)
        return false;
    if (GetDataSize() < (start_index + need_len) or dest == nullptr)
        return false;

    size_t peek_head = Mask(m_Head + start_index);

    // 如果缓冲区末尾剩余空间不够一次性读取，则只复制到缓冲区末尾（巧妙避开环的判定）。
    const size_t firstCopyLen = std::min(m_Capacity - peek_head, need_len);
    std::memcpy(dest, &m_Buf[peek_head], firstCopyLen);
    const size_t secondCopyLen = need_len - firstCopyLen;
    if (secondCopyLen > 0)
        std::memcpy(static_cast<char*>(dest) + firstCopyLen, &m_Buf[0], secondCopyLen);

    return true;
}

bool RingBuffer::PeekToString(int start_index, std::string& dest, size_t need_len) const
{
    if (start_index < 0)
        return false;
    if (need_len == 0)
        return false;
    if (GetDataSize() < (start_index + need_len))
        return false;

    dest.resize(need_len);

    size_t peek_head = Mask(m_Head + start_index);

    // 如果缓冲区末尾剩余空间不够一次性读取，则只复制到缓冲区末尾（巧妙避开环的判定）。
    const size_t firstCopyLen = std::min(m_Capacity - peek_head, need_len);
    std::memcpy(dest.data(), &m_Buf[peek_head], firstCopyLen);
    const size_t secondCopyLen = need_len - firstCopyLen;
    if (secondCopyLen > 0)
        std::memcpy(dest.data() + firstCopyLen, &m_Buf[0], secondCopyLen);

    return true;
}

bool RingBuffer::AppendDataFromCBuffer(const void* src_buf, size_t data_len)
{
    if (data_len <= 0)
        return false;
    if (src_buf == nullptr)
        return false;
    if (not HaveEnoughFreeSpace(data_len))
        TryMakeEnoughFreeSpace(data_len);

    // 如果缓冲区末尾剩余空间不够一次性读取，则只复制到缓冲区末尾（巧妙避开环的判定）。
    const size_t firstCopyLen = std::min(m_Capacity - m_Tail, data_len);
    std::memcpy(GetFreeBegin(), src_buf, firstCopyLen);
    const size_t secondCopyLen = data_len - firstCopyLen;
    if (secondCopyLen > 0)
        std::memcpy(GetBufBegin(), static_cast<const char*>(src_buf) + firstCopyLen, secondCopyLen);

    MoveTail(data_len);

    return true;
}

bool RingBuffer::AppendDataFromProtobuf(const google::protobuf::Message& message)
{
    size_t message_size = message.ByteSizeLong();

    if (not HaveEnoughFreeSpace(message_size))
        TryMakeEnoughFreeSpace(message_size);

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

bool RingBuffer::PopDataToCBuffer(void* dest, size_t need_len)
{
    if (need_len <= 0)
        return false;
    if (not HaveEnoughDataSpace(need_len) or dest == nullptr)
        return false;

    // 如果缓冲区末尾剩余空间不够一次性读取，则只复制到缓冲区末尾（巧妙避开环的判定）。
    const size_t firstCopyLen = std::min(m_Capacity - m_Head, need_len);
    std::memcpy(dest, GetDataBegin(), firstCopyLen);
    const size_t secondCopyLen = need_len - firstCopyLen;
    if (secondCopyLen > 0)
        std::memcpy(static_cast<char*>(dest) + firstCopyLen, GetBufBegin(), secondCopyLen);

    MoveHeadAndTryReset(need_len);

    return true;
}

bool RingBuffer::PopDataToProtobuf(const std::shared_ptr<google::protobuf::Message> & outMsg, size_t message_size)
{
    if (!outMsg)
        return false;
    // if (message_size == 0)
    //     return true;

    if (not HaveEnoughDataSpace(message_size))
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
    if (len == 0)
        return {};
    if (GetDataSize() < len) {
        len = GetDataSize();
    }

    std::string result(len, '\0'); // 预分配字符串空间，注意字符串内容是未初始化的

    if (!PopDataToCBuffer(result.data(), len))
        return {}; // 读取失败返回空字符串

    return result;
}


std::string RingBuffer::PopAllDataAsString()
{
    return PopDataAsString(GetDataSize());
}

bool RingBuffer::TryMakeEnoughFreeSpace(size_t needLen)
{
    if (GetFreeSize() < needLen) {
        // 把数据放入新数组中
        auto new_datasize = GetDataSize() + needLen;
        auto new_capacity = NextPowerOfTwo(new_datasize);
        std::vector<char> new_buffer(new_capacity);

        // 旧数据复制到新数组
        PeekToCBuffer(0, new_buffer.data(), GetDataSize());

        m_Buf = std::move(new_buffer);
        m_Head = 0;
        m_Tail = m_Head + new_datasize;
        m_Capacity = new_capacity;
    }

    return true; //! 无上限扩容
}

void RingBuffer::MoveHeadAndTryReset(size_t offset)
{
    m_Head = Mask(m_Head + offset);
    // 下面的不是必须的
    if(GetDataSize() == 0) {
        Reset();
    }
}
}
