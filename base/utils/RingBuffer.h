#pragma once
#include "copyable.h"

#include <string>
#include <string_view>
#include <memory>
#include <vector>

namespace google::protobuf {
class Message;
}

namespace yy::util
{

class RingBuffer: copyable  {
public:
    static size_t NextPowerOfTwo(const size_t n)
    {
        if (n == 0) return 1;
        // 如果 n 本身是2的幂，则直接返回 n
        if ((n & (n - 1)) == 0)
            return n;

        size_t power = 1;
        while (power < n)
            power <<= 1;
        return power;
    }


    explicit RingBuffer(size_t _capacity)
        : m_Capacity(NextPowerOfTwo(_capacity)), m_Buf(m_Capacity), m_Head(0), m_Tail(0) {}
    RingBuffer(RingBuffer &&) = default;
    RingBuffer &operator=(RingBuffer &&) = default;
    RingBuffer(const RingBuffer &) = default;
    RingBuffer &operator=(const RingBuffer &) = default;
    ~RingBuffer() = default;

    size_t GetHead()   const { return m_Head; }
    size_t GetTail()   const { return m_Tail; }
    size_t GetCapacity() const { return m_Capacity; }

    ///@brief 已填充的字节数
    size_t GetDataSize() const
    {
        if (m_Tail >= m_Head)
            return m_Tail - m_Head;
        else
            return m_Capacity - (m_Head - m_Tail) ;
    }

    ///@brief 未填充的字节数
    size_t GetFreeSize() const
    {
        return m_Capacity - 1 - GetDataSize() ;
    }

    ///@brief
    bool IsDataEmpty() const { return m_Head == m_Tail; }

    ///@brief
    bool IsDataFull() const
    {
        // 满的条件是：写指针的下一个位置是读指针
        return Mask(m_Tail + 1) == m_Head;
    }


    bool HaveEnoughFreeSpace(const int len) const { return GetFreeSize() >= len; }
    bool HaveEnoughDataSpace(const int len) const { return GetDataSize() >= len; }

    void Reset() { m_Head = m_Tail = 0;  }


    ///Region 观察数据
    bool PeekToCBuffer(int start_index, void *dest, size_t need_len) const;

    bool PeekToString(int start_index, std::string & dest, size_t need_len) const;

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
    bool PeekToPodStruct(int start_index, T &dest) const
    {
        return PeekToCBuffer(start_index, &dest, sizeof dest);
    }
    /// End


    ///Region 填充数据（生产数据）
    ///@brief 从C语言的缓冲区`src_buf`中读入`data_len`长度的数据
    bool AppendDataFromCBuffer(const void* src_buf, size_t data_len);

    bool AppendDataFromArray(const std::string_view &message)
    {
        return AppendDataFromCBuffer(message.data(), message.size());
    }

    ///@brief 读取protobuf对象到缓冲区中 ———— sendBuf封装消息体
    bool AppendDataFromProtobuf(const google::protobuf::Message &message);


    ///@brief 从类型`T`的结构src读入数据到缓冲区中 ———— sendBuf封装消息头
    template<class T>
    requires requires {
        requires std::is_standard_layout_v<T> && std::is_trivial_v<T>;
    }
    bool AppendDataFromPODStruct(const T& src)
    {
        return AppendDataFromCBuffer(&src, sizeof(src));
    }
    /// End


    /// Region 取出数据（消费数据）
    ///@brief 将`data_len`长度的数据写入C语言的缓冲区`src_buf`中
    bool PopDataToCBuffer(void* dest, size_t need_len);

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
    bool PopDataToProtobuf(const std::shared_ptr<google::protobuf::Message>& outMsg, size_t message_size);


    ///@brief 读取len长度的数据到string中并返回
    std::string PopDataAsString(size_t len);

    ///@brief 将所有数据读入string中并返回
    std::string PopAllDataAsString();

    void PopData(const size_t offset) { m_Head = Mask(m_Head + offset); }
    /// End


protected:
    // 整体缓冲区：可读写
    char*       GetBufBegin()       { return m_Buf.data(); }
    const char* GetBufBegin() const { return m_Buf.data(); }

    // 数据区：只读
    const char* GetDataBegin() const { return GetBufBegin() + m_Head; }
    const char* GetDataEnd()   const { return GetBufBegin() + m_Tail; }

    // 空闲区：只写
    char* GetFreeBegin() { return GetBufBegin() + m_Tail; }

    bool TryMakeEnoughFreeSpace(size_t needLen);
    bool Compact(int needLen) { return true; }
    void MoveHeadAndTryReset(size_t offset);

    void MoveTail(const size_t offset)
    {
        m_Tail = Mask(m_Tail + offset);
    }

    size_t Mask(const size_t value) const {
        return value & (m_Capacity - 1);
    }


    size_t            m_Capacity{ };
    std::vector<char> m_Buf{ };
    size_t            m_Head{ }; // 消费者指针：用于读取，head是数据区的第一个字节
    size_t            m_Tail{ }; // 生产者指针：用于接收，tail是空闲区的第一个字节
};

}
