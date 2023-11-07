#ifndef ____USERBUFFER_H
#define ____USERBUFFER_H

#include <cstring>
#include <atomic>
#include <google/protobuf/message.h>
#include <wrap_socket.h>


// #define USE_RINGBUFFER


namespace yy::core {

/// @brief 套接字用户缓冲区：单生产者，单消费者
class UserBuffer
{
private:
    char* const buf{ };
    size_t head{ }; // 消费者指针：用于读取，head是数据区的第一个字节
    size_t tail{ }; // 生产者指针：用于接收，tail是空闲区的第一个字节
    std::atomic<bool> isCompleted{ }; // 为true时，表示一批数据接收完毕，可以解析处理(该标志对发送数据没有影响)
    const size_t maxsize{ };

public:
    explicit UserBuffer(size_t _maxsize) : buf(new char[_maxsize]{ }), maxsize{_maxsize}
    {
        Reset();
    }

    ~UserBuffer()
    {
        delete[] buf;
    }

    char*  head_addr() const { return buf + head; }
    char*  tail_addr() const { return buf + tail; }
    size_t get_head() const { return head; }
    size_t get_tail() const { return tail; }

    const char * get_rawpointer() const { return buf; }

    void set_isCompleted(bool value) { isCompleted = value; }
    bool get_isCompleted() const { return isCompleted; }

    void Reset()
    {
        head = tail = 0;
        isCompleted = false;
    }

    void MoveHead(size_t offset) { head = (head + offset) % maxsize; }
    void BackHead(size_t offset) { head = (head - offset) % maxsize; }
    void MoveTail(size_t offset) { tail = (tail + offset) % maxsize; }
    void BackTail(size_t offset) { tail = (tail - offset) % maxsize; }

    ///@brief 已填充的字节数
    size_t DataSize() const { return (tail - head + maxsize) % maxsize; }

    ///@brief 未填充的字节数
    size_t RemainedSize() const { return Maxsize() - DataSize() - 1; }

    ///@brief
    size_t Maxsize() const { return maxsize; }

    ///@brief
    bool IsEmpty() const { return DataSize() == 0; }

    ///@brief
    bool IsFull() const { return RemainedSize() == 0; }

    //! 填充空闲区
    ///@brief 从C语言的缓冲区`src_buf`中读入`data_len`长度的数据 ———— recvBuf从临时缓冲区读取数据
    bool ReadFromCBuffer(const void* src_buf, size_t data_len) //NOLINT
    {
        if(RemainedSize() < data_len)
            return false;

#ifdef USE_RINGBUFFER
        size_t part1_len = maxsize - tail;
        int part2_len = (int)(data_len - part1_len);
        if(part2_len > 0)
        {
            memcpy(tail_addr(), src_buf, part1_len);
            MoveTail(part1_len);
            memcpy(tail_addr(), (char*)src_buf+part1_len, part2_len);
            MoveTail(part2_len);
        }
        else
#endif
        {
            memcpy(tail_addr(), src_buf, data_len);
            MoveTail(data_len);
        }

        return true;
    }

    ///@brief 从类型`T`的结构src读入数据到缓冲区中 ———— sendBuf封装消息头
    template<class T>
    bool ReadFromStruct(const T& src)
    {
        return ReadFromCBuffer(&src, sizeof(src));
    }

    ///@brief 读取protobuf对象到缓冲区中 ———— sendBuf封装消息体
    bool ReadFromProtobuf(const google::protobuf::Message* src_msg)
    {
        if(RemainedSize() < src_msg->ByteSizeLong())
            return false;
#ifdef USE_RINGBUFFER
        size_t part1_len = maxsize - tail;
        if((src_msg->ByteSizeLong() - part1_len) > 0)
        {
            // protobuf -> temp_arr -> buf
            char temp_arr[src_msg->ByteSizeLong()];
            src_msg->SerializeToArray(temp_arr, (int)src_msg->ByteSizeLong()); // protobuf -> temp_arr
            return ReadFromCBuffer(temp_arr, src_msg->ByteSizeLong()); // temp_arr -> buf
        }
        else
#endif
        {
            src_msg->SerializeToArray(tail_addr(), (int)src_msg->ByteSizeLong());
            MoveTail(src_msg->ByteSizeLong());
        }

        return true;
    }
    bool ReadFromProtobuf(const google::protobuf::Message& src_msg)
    {
        return ReadFromProtobuf(&src_msg);
    }



    //! 读取数据区
    ///@brief 将`data_len`长度的数据写入C语言的缓冲区`src_buf`中
    bool WriteToCBuffer(void* dest_buf, size_t data_len) //NOLINT
    {
        if(DataSize() < data_len)
            return false;

#ifdef USE_RINGBUFFER
        size_t part1_len = maxsize - head;
        int part2_len = (int)(data_len - part1_len);
        if(part2_len > 0)
        {
            memcpy(dest_buf, head_addr(), part1_len);
            MoveHead(part1_len);
            memcpy((char*)dest_buf+part1_len, head_addr(), part2_len);
            MoveHead(part2_len);
        }
        else
#endif
        {
            memcpy(dest_buf, head_addr(), data_len);
            MoveHead(data_len);
        }

        return true;
    }

    ///@brief 将大小为类型T的数据写入类型`T`的结构 ———— 从recvBuf读取数据到消息头结构体中
    template<class T>
    bool WriteToStruct(T& dest)
    {
        return WriteToCBuffer(&dest, sizeof(dest));
    }

    ///@brief 将缓冲区的数据写入protobuf对象，调用者须知道protobuf对象的实际长度 ———— 从recvBuf读取数据到protobuf消息中
    bool WriteToProtobuf(google::protobuf::Message* dst_msg, size_t len)
    {
        if(DataSize() < len)
            return false;

#ifdef USE_RINGBUFFER
        size_t part1_len = maxsize - head;
        if(len - part1_len > 0)
        {
            // buf -> temp_arr -> dst_msg
            char temp_arr[len];
            bool isOk = WriteToCBuffer(temp_arr, len); // buf -> temp_arr
            dst_msg->ParseFromArray(temp_arr, (int)len); // temp_arr -> dst_msg
            return isOk;
        }
        else
#endif
        {
            dst_msg->ParseFromArray(head_addr(), (int)(len));
            MoveHead(len);
        }

        return true;
    }
    bool WriteToProtobuf(google::protobuf::Message & dst_msg, size_t len)
    {
        return WriteToProtobuf(&dst_msg, len);
    }


    ssize_t WriteToSocket(util::Socket sock)
    {
        // *return：没有可以发送的数据
        if(DataSize() <= 0)
            return 0;

        ssize_t nBytesSend;

#ifdef USE_RINGBUFFER
        if(head > tail)
        {
            char temp_buf[DataSize()];
            bool isOk = WriteToCBuffer(temp_buf, DataSize());
            if(!isOk)
                return 0;
            nBytesSend = sock.Send(temp_buf, sizeof(temp_buf), 0);

            if(nBytesSend > 0) {
                set_isCompleted(true);
            } else {
                return -1;
            }
        }
        else
#endif
        {
            nBytesSend = sock.Send(head_addr(), DataSize(), 0);

            if(nBytesSend > 0) {
                MoveHead(nBytesSend);
                set_isCompleted(true);
            } else {
                return -1;
            }
        }

        return nBytesSend;
    }


    void print() const
    {
        for(int i = 0; i < Maxsize() ;++i)
        {
            if(get_rawpointer()[i] == '\0')
                std::cout << "[ ]";
            else if(isprint(get_rawpointer()[i]))
            {
                std::cout << "[ ]";
            }
            else if(get_rawpointer()[i] == '\b')
            {
                std::cout << "[ ]";
            }
            else
                std::cout << "[" << get_rawpointer()[i] << "]";
        } std::cout << "\n";
        size_t head_pos = get_head() * 3 + 2;
        size_t tail_pos = head_pos + (get_tail() - get_head()) * 3;

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


};

}



#endif //____USERBUFFER_H
