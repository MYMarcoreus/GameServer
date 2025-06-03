#include "SequentialBuffer.h"
#include "util_functions.h"
#include <google/protobuf/message.h>

namespace yy::util {
SequentialBuffer::SequentialBuffer(const size_t _capacity)
    : m_Buf(_capacity, 0),
    m_Capacity{_capacity},
    m_Head{0},
    m_Tail{0}
{ }


bool SequentialBuffer::AppendDataFromCBuffer(const void *src_buf, size_t data_len) {
    bool isEnsured = TryMakeEnoughFreeSpace(data_len);
    std::memcpy(GetFreeBegin(), src_buf, data_len);
    MoveTail(data_len);
    return isEnsured;
}


bool SequentialBuffer::AppendDataFromArray(const std::string_view &message) {
    bool isEnsured = TryMakeEnoughFreeSpace(message.size());
    std::memcpy(GetFreeBegin(), message.data(), message.size());
    MoveTail(message.size());
    return isEnsured;
}



bool SequentialBuffer::AppendDataFromProtobuf(const google::protobuf::Message & src_msg) {
    bool isEnsured = TryMakeEnoughFreeSpace(src_msg.ByteSizeLong());
    if (!src_msg.SerializeToArray(GetFreeBegin(), static_cast<int>(src_msg.ByteSizeLong())))
        return false;
    MoveTail(src_msg.ByteSizeLong());
    return isEnsured;
}

bool SequentialBuffer::TryMakeEnoughFreeSpace(size_t needLen) {
    // 空闲空间不足，尝试释放空间
    if(not Compact(needLen)) {
        // 若释放空间后仍无法放入数据，则需要扩容buf
        //todo 扩容是否存在上限？若无上限，则是否考虑缩容？
        m_Capacity = m_Capacity + (needLen - GetFreeSize());
        m_Buf.resize(m_Capacity);
    }

    return true; //! 无上限扩容
}

bool SequentialBuffer::Compact(size_t needLen) {
    int dataSize = GetDataSize();
    // 将数据区移到缓冲区最前，回收DataBegin()前的空间（如果数据区已在最前，则不移动）
    if(dataSize > 0 and m_Head != 0) {
        std::copy(GetDataBegin(), GetDataEnd(), GetBufBegin());
    }
    m_Head = 0;
    m_Tail = m_Head + dataSize;

    //todo 如果扩容不存在上限，那么是否需要缩容呢？例如：如果needLen小于回收操作后空闲空间的大小的一半、或者过一段时间(若干接受数据包之后)之后进行缩容

    return GetFreeSize() >= needLen;
}

bool SequentialBuffer::PopDataToCBuffer(void *dest_buf, size_t data_len) //NOLINT
{
    if(!HaveEnoughDataSpace(data_len)) {
        return false;
    }

    memcpy(dest_buf, GetDataBegin(), data_len);
    MoveHeadAndTryReset(data_len);

    return true;
}

bool SequentialBuffer::PopDataToProtobuf(const std::shared_ptr<google::protobuf::Message> & outMsg, const size_t len) {
    if(!HaveEnoughDataSpace(len))
        return false;

    if(outMsg)
        outMsg->ParseFromArray(GetDataBegin(), static_cast<int>(len));
    else
        return false;
    MoveHeadAndTryReset(len);

    return true;
}



std::string SequentialBuffer::PopDataAsString(size_t len) {
    if(!HaveEnoughDataSpace(len))
        return "";

    std::string ret{GetDataBegin(), GetDataBegin() + len};
    MoveHeadAndTryReset(len);

    return ret;
}

std::string SequentialBuffer::PopAllDataAsString() {
    return PopDataAsString(GetDataSize());
}



void SequentialBuffer::MoveHeadAndTryReset(size_t offset) {
    m_Head += offset;
    if(GetDataSize() == 0) {
        Reset();
    }
}



bool SequentialBuffer::PeekToCBuffer(const int start_index, void *dest, const size_t len) const {
    if(GetDataSize() < len)
        return false;
    memcpy(dest, GetDataBegin()+start_index, len);
    return true;
}

bool SequentialBuffer::PeekToString(const int start_index, std::string & dest, size_t len) const {
     if(GetDataSize() < len)
        return false;
    dest.assign(GetDataBegin()+start_index, len);
    return true;
}


void SequentialBuffer::Print() const {
    for(int i = 0; i < GetCapacity() ;++i)
    {
        if(Peek()[i] == '\0')
            std::cout << "[ ]";
        else if(isprint(Peek()[i]))
        {
            std::cout << "[ ]";
        }
        else if(Peek()[i] == '\b')
        {
            std::cout << "[ ]";
        }
        else
            std::cout << "[" << Peek()[i] << "]";
    } std::cout << "\n";
    size_t head_pos = GetHead() * 3 + 2;
    size_t tail_pos = head_pos + (GetTail() - GetHead()) * 3;

    if(head_pos < tail_pos)
    {
        std::string blank1(head_pos-1, ' ');
        std::string blank2(tail_pos-head_pos-2, ' ');
        std::cout << blank1 << "^h" << blank2 << "^t" << "\n";
    }
    else if(head_pos == tail_pos)
    {
        std::string blank1(head_pos-1, ' ');
        std::cout << blank1 << "^ht" << "\n";
    }
    else
    {
        std::string blank1(tail_pos-1, ' ');
        std::string blank2(head_pos-tail_pos-2, ' ');
        std::cout << blank1 << "^t" << blank2 << "^h" << "\n";
    }
}




}
