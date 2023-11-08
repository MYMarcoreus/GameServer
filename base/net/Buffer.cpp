#include <sys/socket.h>
#include "Buffer.h"

namespace yy::net {
Buffer::Buffer(size_t _maxsize)
    : m_Buf(_maxsize, 0),
    m_Maxsize{_maxsize},
    m_Head{0},
    m_Tail{0}
{ }


bool Buffer::AppendDataFromCBuffer(const void *src_buf, size_t data_len) //NOLINT
{
    if(!TestAndSetFreeSpace(data_len)) {
        return false;
    }

    memcpy(GetFreeBegin(), src_buf, data_len);
    MoveTail(data_len);

    return true;
}

bool Buffer::AppendDataFromProtobuf(const std::shared_ptr<google::protobuf::Message> & src_msg) {
    if(!TestAndSetFreeSpace(src_msg->ByteSizeLong())) {
        return false;
    }

    src_msg->SerializeToArray(GetFreeBegin(), (int)src_msg->ByteSizeLong());
    MoveTail(src_msg->ByteSizeLong());

    return true;
}



bool Buffer::RetrieveDataIntoCBuffer(void *dest_buf, size_t data_len) //NOLINT
{
    if(!HaveEnoughData(data_len)) {
        return false;
    }

    memcpy(dest_buf, GetDataBegin(), data_len);
    MoveHeadAndTryReset(data_len);

    return true;
}

bool Buffer::RetrieveDataIntoProtobuf(const std::shared_ptr<google::protobuf::Message> & outMsg, size_t len) {
    if(!HaveEnoughData(len))
        return false;

    if(outMsg)
        outMsg->ParseFromArray(GetDataBegin(), (int)(len));
    else
        return false;
    MoveHeadAndTryReset(len);

    return true;
}

ssize_t Buffer::RetrieveDataIntoSocket(SocketApiWrapper::socket_t sockfd) {
    // *return：没有可以发送的数据
    if(GetDataSize() <= 0)
        return 0;

    ssize_t nBytesSend = ::send(sockfd, GetDataBegin(), GetDataSize(), 0);

    if(nBytesSend > 0) {
        MoveHeadAndTryReset(nBytesSend);
    }

    return nBytesSend;
}

std::string Buffer::RetrieveDataAsString(int len) {
    if(!HaveEnoughData(len))
        return "";

    std::string ret{GetDataBegin(), GetDataBegin() + len};
    MoveHeadAndTryReset(len);

    return ret;
}

std::string Buffer::RetrieveAllDataAsString() {
    return RetrieveDataAsString(GetDataSize());
}

bool Buffer::TestAndSetFreeSpace(int needLen) {
    // 空闲空间不足
    if(GetFreeSize() < needLen) {
        int dataSize = GetDataSize();
        // 将数据区移到缓冲区最前，回收DataBegin()前的空间
        if(dataSize > 0) {
            std::copy(GetDataBegin(), GetFreeBegin(), GetBufBegin());
        }
        m_Head = 0;
        m_Tail = m_Head + dataSize;

        // DataBegin()前的空间 + FreeBegin()后的空间 >= 所需的空间
        return GetMaxsize() - GetDataSize() >= needLen;
    }
        // 空闲空间足够
    else {
        return true;
    }
}

void Buffer::MoveHeadAndTryReset(size_t offset) {
    m_Head += offset;
    if(GetDataSize() == 0) {
        Reset();
    }
}

bool Buffer::AppendDataFromProtobuf(const google::protobuf::Message &src_msg) {
    if(!TestAndSetFreeSpace(src_msg.ByteSizeLong())) {
        return false;
    }

    src_msg.SerializeToArray(GetFreeBegin(), (int)src_msg.ByteSizeLong());
    MoveTail(src_msg.ByteSizeLong());

    return true;
}

bool Buffer::PeekCBuffer(int start_index, void *dest, int len) {
    if(GetDataSize() < len)
        return false;
    memcpy(dest, GetDataBegin()+start_index, len);
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