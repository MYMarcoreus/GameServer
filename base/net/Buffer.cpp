#include "Buffer.h"
#include "SocketApiWrapper.h"
#include "Socket.h"
#include <google/protobuf/message.h>

namespace yy::net {
Buffer::Buffer(size_t _maxsize)
    : m_Buf(_maxsize, 0),
    m_Maxsize{_maxsize},
    m_Head{0},
    m_Tail{0}
{ }


#pragma clang diagnostic push
#pragma ide diagnostic ignored "ConstantFunctionResult"
bool Buffer::AppendDataFromCBuffer(const void *src_buf, size_t data_len) {
    bool isEnsured = TryMakeEnoughSpace(data_len);
    std::memcpy(GetFreeBegin(), src_buf, data_len);
    MoveTail(data_len);
    return isEnsured;
}
#pragma clang diagnostic pop


#pragma clang diagnostic push
#pragma ide diagnostic ignored "ConstantFunctionResult"
bool Buffer::AppendDataFromArray(const std::string_view &message) {
   bool isEnsured = TryMakeEnoughSpace(message.size());
    std::memcpy(GetFreeBegin(), message.data(), message.size());
    MoveTail(message.size());
    return isEnsured;
}
#pragma clang diagnostic pop


#pragma clang diagnostic push
#pragma ide diagnostic ignored "ConstantFunctionResult"
bool Buffer::AppendDataFromProtobuf(const google::protobuf::Message & src_msg) {
    bool isEnsured = TryMakeEnoughSpace(src_msg.ByteSizeLong());
    src_msg.SerializeToArray(GetFreeBegin(), (int)src_msg.ByteSizeLong());
    MoveTail(src_msg.ByteSizeLong());
    return isEnsured;
}
#pragma clang diagnostic pop


bool Buffer::RecvFromSocket(const std::unique_ptr<Socket> &sock, const size_t nBytesRecvOnce, SocketApiWrapper::SocketResult &rst, const std::shared_ptr<IPAddress>& peerAddr) {
    Compact(GetMaxsize());

    switch (sock->GetType()) {

        case Socket::Type::TCP:
            rst = sock->Recv(GetFreeBegin(), nBytesRecvOnce);
            break;
        case Socket::Type::UDP:
            rst = sock->Recvfrom(GetFreeBegin(), nBytesRecvOnce, 0, peerAddr);
            break;
    }
    if (rst.HasNoError()) {
        MoveTail(rst.Result());
    }
    return true;
}

bool Buffer::RecvAllFromSocket(const std::unique_ptr<Socket> &sock, SocketApiWrapper::SocketResult & rst, const std::shared_ptr<IPAddress>& peerAddr) {
    Compact(GetMaxsize());
    const uint32_t writable =  static_cast<uint32_t>(GetFreeSize());

    // 应该的写法：执行系统调用 ioctl(fd, FIONREAD, out datasize) 来获取可读字节数，以此来精确分配缓冲区空间(需要malloc动态分配)并选择是否需要创建第二缓冲区。
    // const int iovcnt = (writable < datasize) ? 2 : 1;
    // 目前的写法：使用栈上分配的静态数据，在编译时已知，因此在函数调用的栈帧中被预留了空间，比动态分配malloc快很多。
    char tempBuf[65536];

    IOV_TYPE vec[2];
    vec[0].IOV_PTR_FIELD = GetFreeBegin();
    vec[0].IOV_LEN_FIELD = writable;
    vec[1].IOV_PTR_FIELD = tempBuf;
    vec[1].IOV_LEN_FIELD = sizeof tempBuf;
    constexpr int iovcnt = 2;

    bool isOk = true;

    switch (sock->GetType()) {
        case Socket::Type::TCP:
            rst = sock->Readv(vec, iovcnt);
            break;
        case Socket::Type::UDP:
            rst = sock->Readmsg(vec, iovcnt, peerAddr);
            break;
    }

    if (rst.HasNoError())
    {
        const ssize_t n = rst.Result();
        //! 一个缓冲区足矣读取完所有数据
         if(n <= writable) {
            MoveTail(n);
            isOk = true;
        }
        //! 数据在两个缓冲区中
        else {
            //! 处理第一个缓冲区
            MoveTail(writable);
            //! 将第二个缓冲区的数据加入第一个缓冲区(第一个缓冲区有自动扩容机制)
            auto tempBufSize = n - writable;
            isOk = AppendDataFromCBuffer(tempBuf, tempBufSize); //! 若有限制扩容机制，则需要处理返回值
        }
    }

    // isOk为false时说明客户端发送数据过快大于限制的缓冲区最大值
    return isOk;
}





#pragma clang diagnostic push
#pragma ide diagnostic ignored "ConstantFunctionResult"
bool Buffer::TryMakeEnoughSpace(int needLen) {
    // 空闲空间不足，尝试释放空间
    if(not Compact(needLen)) {
        // 若释放空间后仍无法放入数据，则需要扩容buf
        //todo 扩容是否存在上限？若无上限，则是否考虑缩容？
        m_Buf.resize(GetDataSize() + needLen);
    }

    return true; //! 无上限扩容
}
#pragma clang diagnostic pop

bool Buffer::Compact(int needLen) {
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
    if(!HaveEnoughData(data_len)) {
        return false;
    }

    memcpy(dest_buf, GetDataBegin(), data_len);
    MoveHeadAndTryReset(data_len);

    return true;
}

bool Buffer::PopDataToProtobuf(const std::shared_ptr<google::protobuf::Message> & outMsg, size_t len) {
    if(!HaveEnoughData(len))
        return false;

    if(outMsg)
        outMsg->ParseFromArray(GetDataBegin(), (int)(len));
    else
        return false;
    MoveHeadAndTryReset(len);

    return true;
}

SocketApiWrapper::SocketResult Buffer::SendToSocket(std::unique_ptr<Socket> &sock, std::shared_ptr<IPAddress> peerAddr) {
    // *return：没有可以发送的数据
    if(GetDataSize() <= 0)
        return 0;

    SocketApiWrapper::SocketResult rst;
    switch (sock->GetType()) {
        case Socket::Type::TCP:
            rst = sock->Send(GetDataBegin(), GetDataSize(), 0);
            break;
        case Socket::Type::UDP:
            rst = sock->Sendto(GetDataBegin(), GetDataSize(), 0, peerAddr);
            break;
    }

    if(rst.HasNoError()) {
        MoveHeadAndTryReset(rst.Result());
    }

    return rst;
}

std::string Buffer::PopDataAsString(int len) {
    if(!HaveEnoughData(len))
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



bool Buffer::PeekToCBuffer(int start_index, void *dest, int len) const {
    if(GetDataSize() < len)
        return false;
    memcpy(dest, GetDataBegin()+start_index, len);
    return true;
}

bool Buffer::PeekToString(int start_index, std::string & dest, int len) const {
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
