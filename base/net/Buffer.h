#ifndef LINUXGAMESERVER_BUFFER_H
#define LINUXGAMESERVER_BUFFER_H

#include "socket_definations.h"
#include "net_definations.h"
#include <cstring>
#include <atomic>
#include <google/protobuf/message.h>

namespace yy::net {

class Buffer: util::copyable {
public:
    explicit Buffer(size_t _maxsize);
    Buffer(Buffer &&) = default;
    Buffer &operator=(Buffer &&) = default;
    Buffer(const Buffer &) = default;
    Buffer &operator=(const Buffer &) = default;
    ~Buffer() = default;

    size_t GetHead()   const { return m_Head; }
    size_t GetTail()   const { return m_Tail; }

    ///@brief 已填充的字节数
    size_t GetDataSize() const {
        assert(m_Tail >= m_Head);
        return GetTail() - GetHead();
    }

    ///@brief 未填充的字节数
    size_t GetFreeSize() const {
        auto rst = GetMaxsize() - GetTail();
        assert(rst >= 0);
        return rst;
    }

    ///@brief
    size_t GetMaxsize() const { return m_Maxsize; }

    ///@brief
    bool IsDataEmpty() const { return GetDataSize() == 0; }

    ///@brief
    bool IsDataFull() const { return GetFreeSize() == 0; }


    /*
     * concept and requires：
     * 第一个requires是约束，后面跟着concept，
     * 第二个 requires { ... } 代表concept
     * {}内部的requires 是concept的约束
     * */
    template<class T>
    requires requires {
        requires std::is_standard_layout_v<T>;
        requires std::is_trivial_v<T>;
    }
    T PeekPodStruct(int start_index)
    {
        assert(GetDataSize() >= sizeof(T));
        return *(T*)(m_Buf.data()+m_Head+start_index);
    }

    template<class T>
    requires requires {
        requires std::is_standard_layout_v<T>;
        requires std::is_trivial_v<T>;
    }
    bool PeekPodStruct(int start_index, T &dest)
    {
        if(GetDataSize() < sizeof(T))
            return false;
        memcpy(&dest, GetDataBegin()+start_index, sizeof(dest));
        return true;
    }

    bool PeekCBuffer(int start_index, void *dest, int len);

    const char * Peek(int start = 0) const { return GetBufBegin() + m_Head + start; }



    //! 填充数据（生产数据）
    ///@brief 从C语言的缓冲区`src_buf`中读入`data_len`长度的数据 ———— recvBuf从临时缓冲区读取数据
    bool AppendDataFromCBuffer(const void* src_buf, size_t data_len);

    ///@brief 从类型`T`的结构src读入数据到缓冲区中 ———— sendBuf封装消息头
    template<class T>
    requires requires {
        requires std::is_standard_layout_v<T> && std::is_trivial_v<T>;
    }
    bool AppendDataFromPODStruct(const T& src)
    {
        return AppendDataFromCBuffer(&src, sizeof(src));
    }

    ///@brief 读取protobuf对象到缓冲区中 ———— sendBuf封装消息体
    bool AppendDataFromProtobuf(const std::shared_ptr<google::protobuf::Message> & src_msg);
    bool AppendDataFromProtobuf(const google::protobuf::Message & src_msg);

    /*! Buffer不实现来自套接字Socket的recv任务，因为对于recv任务，存在ET和LT的区别，因此原样recv的错误，让其所有者TcpConnection实现 !*/
    // ET
    bool AppendDataFromSocket(std::unique_ptr<Socket> & sock, size_t nBytesRecvOnce, SocketApiWrapper::SocketResult & rst);
    // LT
    bool AppendAllDataFromSocket(std::unique_ptr<Socket> & sock, SocketApiWrapper::SocketResult & rst);


    //! 取出数据（消费数据）
    ///@brief 将`data_len`长度的数据写入C语言的缓冲区`src_buf`中
    bool PopDataToCBuffer(void* dest_buf, size_t data_len);
    ///@brief 将大小为类型T的数据写入类型`T`的结构  ———— 从recvBuf读取数据到消息头结构体中
    template<class T>
    requires requires {
        requires std::is_standard_layout_v<T>;
        requires std::is_trivial_v<T>;
    }
    bool PopDataToPODStruct(T& dest)
    {
        return PopDataToCBuffer(&dest, sizeof(dest));
    }

    ///@brief 将缓冲区的数据写入protobuf对象，调用者须知道protobuf对象的实际长度 ———— 从recvBuf读取数据到protobuf消息中
    bool PopDataToProtobuf(const std::shared_ptr<google::protobuf::Message> & outMsg, size_t len);

    SocketApiWrapper::SocketResult PopDataToSocket(SocketApiWrapper::socket_t sockfd);

    ///@brief 读取len长度的数据到string中并返回
    std::string PopDataAsString(int len);

    ///@brief 将所有数据读入string中并返回
    std::string PopAllDataAsString();

    void MoveHead(size_t offset) { m_Head += offset; }

private:
    char *       GetBufBegin ()       { return &*m_Buf.begin(); }
    const char * GetBufBegin () const { return &*m_Buf.begin(); }

    char * GetFreeBegin() { return GetBufBegin() + m_Tail; }
    char * GetDataBegin() { return GetBufBegin() + m_Head; }
    char * GetDataEnd() { return GetFreeBegin(); }

    bool HaveEnoughData(int len) { return GetDataSize() >= len; }

    bool TryMakeEnoughSpace(int needLen);
    bool TryMakeEnoughFreeSpaceByShifting(int needLen);


    void MoveHeadAndTryReset(size_t offset);

    void BackHead(size_t offset) { m_Head -= offset; }
    void MoveTail(size_t offset) { m_Tail += offset; }
    void BackTail(size_t offset) { m_Tail -= offset; }


    void Reset() {
        m_Head = m_Tail = 0;
    }

    void Print() const;






private:
/// +-------------------+------------------+------------------+
/// | prependable bytes |       数据区      |       空闲区      |
/// |                   |     (GetDataSize)   |     (GetFreeSize)   |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=        m_Head      <=     m_Tail    <=       GetMaxsize
    std::vector<char> m_Buf{ };
    size_t            m_Head{ }; // 消费者指针：用于读取，head是数据区的第一个字节
    size_t            m_Tail{ }; // 生产者指针：用于接收，tail是空闲区的第一个字节
    size_t            m_Maxsize{ };
};

}

#endif //LINUXGAMESERVER_BUFFER_H

