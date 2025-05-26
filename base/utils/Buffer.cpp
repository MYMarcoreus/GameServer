#include "Buffer.h"
#include "util_functions.h"
#include <google/protobuf/message.h>

namespace yy::util {
Buffer::Buffer(const size_t _maxsize)
    : m_Buf(_maxsize, 0),
    m_Maxsize{_maxsize},
    m_Head{0},
    m_Tail{0}
{ }


bool Buffer::AppendDataFromCBuffer(const void *src_buf, size_t data_len) {
    bool isEnsured = TryMakeEnoughSpace(data_len);
    std::memcpy(GetFreeBegin(), src_buf, data_len);
    MoveTail(data_len);
    return isEnsured;
}


bool Buffer::AppendDataFromArray(const std::string_view &message) {
   bool isEnsured = TryMakeEnoughSpace(message.size());
    std::memcpy(GetFreeBegin(), message.data(), message.size());
    MoveTail(message.size());
    return isEnsured;
}



bool Buffer::AppendDataFromProtobuf(const google::protobuf::Message & src_msg) {
    bool isEnsured = TryMakeEnoughSpace(src_msg.ByteSizeLong());
    src_msg.SerializeToArray(GetFreeBegin(), static_cast<int>(src_msg.ByteSizeLong()));
    MoveTail(src_msg.ByteSizeLong());
    return isEnsured;
}

bool Buffer::TryMakeEnoughSpace(const int needLen) {
    // 空闲空间不足，尝试释放空间
    if(not Compact(needLen)) {
        // 若释放空间后仍无法放入数据，则需要扩容buf
        //todo 扩容是否存在上限？若无上限，则是否考虑缩容？
        m_Buf.resize(GetDataSize() + needLen);
    }

    return true; //! 无上限扩容
}

bool Buffer::Compact(const int needLen) {
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

bool Buffer::PopDataToCBuffer(void *dest_buf, size_t data_len) //NOLINT
{
    if(!HaveEnoughDataSpace(data_len)) {
        return false;
    }

    memcpy(dest_buf, GetDataBegin(), data_len);
    MoveHeadAndTryReset(data_len);

    return true;
}

bool Buffer::PopDataToProtobuf(const std::shared_ptr<google::protobuf::Message> & outMsg, const size_t len) {
    if(!HaveEnoughDataSpace(len))
        return false;

    if(outMsg)
        outMsg->ParseFromArray(GetDataBegin(), static_cast<int>(len));
    else
        return false;
    MoveHeadAndTryReset(len);

    return true;
}



std::string Buffer::PopDataAsString(const int len) {
    if(!HaveEnoughDataSpace(len))
        return "";

    std::string ret{GetDataBegin(), GetDataBegin() + len};
    MoveHeadAndTryReset(len);

    return ret;
}

std::string Buffer::PopAllDataAsString() {
    return PopDataAsString(GetDataSize());
}



void Buffer::MoveHeadAndTryReset(size_t offset) {
    m_Head += offset;
    if(GetDataSize() == 0) {
        Reset();
    }
}



bool Buffer::PeekToCBuffer(const int start_index, void *dest, const int len) const {
    if(GetDataSize() < len)
        return false;
    memcpy(dest, GetDataBegin()+start_index, len);
    return true;
}

bool Buffer::PeekToString(const int start_index, std::string & dest, const int len) const {
     if(GetDataSize() < len)
        return false;
    dest.assign(GetDataBegin()+start_index, len);
    return true;
}


void Buffer::Print() const {
    for(int i = 0; i < GetMaxsize() ;++i)
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
