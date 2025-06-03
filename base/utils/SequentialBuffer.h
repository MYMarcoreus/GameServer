#ifndef LINUXGAMESERVER_BUFFER_H
#define LINUXGAMESERVER_BUFFER_H

#include <cstring>
#include <string>
#include <string_view>
#include <memory>
#include <vector>
#include "copyable.h"

namespace google::protobuf {
class Message;
}

namespace yy::util {

// 每一个TcpConnection对应一个recvbuf和sendbuf，而每一个TcpConnection仅仅会被一个ioloop线程操作io，因此Buffer在此场景下线程安全
class SequentialBuffer: copyable {
public:
    explicit SequentialBuffer(size_t _capacity);
    SequentialBuffer(SequentialBuffer &&) = default;
    SequentialBuffer &operator=(SequentialBuffer &&) = default;
    SequentialBuffer(const SequentialBuffer &) = default;
    SequentialBuffer &operator=(const SequentialBuffer &) = default;
    ~SequentialBuffer() = default;

    size_t GetHead()   const { return m_Head; }
    size_t GetTail()   const { return m_Tail; }
    size_t GetCapacity() const { return m_Capacity; }

    ///@brief 已填充的字节数
    size_t GetDataSize() const {
        return GetTail() - GetHead();
    }

    ///@brief 未填充的字节数
    size_t GetFreeSize() const {
        const auto rst = GetCapacity() - GetTail();
        return rst;
    }


    ///@brief
    bool IsDataEmpty() const { return GetDataSize() == 0; }

    ///@brief
    bool IsDataFull() const { return GetFreeSize() == 0; }

    bool HaveEnoughFreeSpace(const int len) const { return GetFreeSize() >= len; }
    bool HaveEnoughDataSpace(const int len) const { return GetDataSize() >= len; }

    void Reset() { m_Head = m_Tail = 0; }

    ///Region 观察数据
    const char * Peek(const int start = 0) const { return GetBufBegin() + m_Head + start; }
    bool PeekToCBuffer(int start_index, void *dest, size_t len) const;

    bool PeekToString(int start_index, std::string & dest, size_t len) const;
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
    bool PeekToPodStruct(const int start_index, T &dest) const
    {
        return PeekToCBuffer(start_index, &dest, sizeof(dest));
    }
    /// End



    ///Region 填充数据（生产数据）
    ///@brief 从C语言的缓冲区`src_buf`中读入`data_len`长度的数据
    bool AppendDataFromCBuffer(const void* src_buf, size_t data_len);

    bool AppendDataFromArray(const std::string_view &message);

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
    bool AppendDataFromProtobuf(const google::protobuf::Message & message);
    /// End



    /// Region 取出数据（消费数据）
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


    ///@brief 读取len长度的数据到string中并返回
    std::string PopDataAsString(size_t len);

    ///@brief 将所有数据读入string中并返回
    std::string PopAllDataAsString();

    void PopData(const size_t offset) { m_Head += offset; }
    /// End

protected:
    // 整体缓冲区：可读写
    char*       GetBufBegin()       { return m_Buf.data(); }
    const char* GetBufBegin() const { return m_Buf.data(); }

    // 数据区：只读
    const char* GetDataBegin() const { return GetBufBegin() + m_Head; }
    const char* GetDataEnd()   const { return GetBufBegin() + m_Tail; }  // 与 GetFreeBegin 一致

    // 空闲区：只写
    char* GetFreeBegin() { return GetBufBegin() + m_Tail; }


    bool TryMakeEnoughSpace(int needLen);
    bool Compact(int needLen);

    void MoveHeadAndTryReset(size_t offset);

    void MoveTail(const size_t offset) { m_Tail += offset; }

    void Print() const;


/// +-------------------+------------------+------------------+
/// | prependable bytes |       数据区      |       空闲区       |
/// |                   |  (GetDataSize)   |  (GetFreeSize)   |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=        m_Head      <=     m_Tail    <=       GetMaxsize
    std::vector<char> m_Buf{ };
    size_t            m_Capacity{ };
    size_t            m_Head{ }; // 消费者指针：用于读取，head是数据区的第一个字节
    size_t            m_Tail{ }; // 生产者指针：用于接收，tail是空闲区的第一个字节
};

}

#endif //LINUXGAMESERVER_BUFFER_H

