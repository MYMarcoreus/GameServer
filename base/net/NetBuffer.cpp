#include "NetBuffer.h"
#include "Buffer.h"
#include "Socket.h"
#include "SocketApiWrapper.h"

namespace yy::net
{
NetBuffer::NetBuffer(const size_t _maxsize): Buffer(_maxsize)
{
}

bool NetBuffer::RecvFromSocket(const std::unique_ptr<Socket> &sock, const size_t nBytesRecvOnce, SocketApiWrapper::SocketResult &rst, const std::shared_ptr<IPAddress>& peerAddr) {
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

bool NetBuffer::RecvAllFromSocket(const std::unique_ptr<Socket> &sock, SocketApiWrapper::SocketResult & rst, const std::shared_ptr<IPAddress>& peerAddr) {
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
            const auto tempBufSize = n - writable;
            isOk = AppendDataFromCBuffer(tempBuf, tempBufSize); //! 若有限制扩容机制，则需要处理返回值
        }
    }

    // isOk为false时说明客户端发送数据过快大于限制的缓冲区最大值
    return isOk;
}


SocketApiWrapper::SocketResult NetBuffer::SendToSocket(const std::unique_ptr<Socket> &sock, std::shared_ptr<IPAddress> peerAddr) {
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

}
