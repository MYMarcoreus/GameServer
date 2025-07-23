#include <iostream>
#include <sstream>
#include <thread>
#include <mutex>
#include "util_functions.h"


size_t GetThreadID_1()
{
    std::thread::id threadId = std::this_thread::get_id();

    //! 线程安全，因为std::hash只是一个仿函数，没有啥数据成员c，不存在资源竞争
    //! 唯一的问题就是返回的是原生线程id的哈希值，而不是原生id值
    static std::hash<std::thread::id> hasher;
    return hasher(threadId);
}

std::thread::native_handle_type GetThreadID_2()
{
    //! 很好，因为std::thread::id类型只有一个数据成员且没有虚函数，可直接取地址获得内部的原生线程id成员
    std::thread::id threadId = std::this_thread::get_id();
    return *(std::thread::native_handle_type*)(&threadId);
}

std::string GetThreadID_3()
{
    std::thread::id threadId = std::this_thread::get_id();

    //! 线程不安全，请不要使用
    static std::ostringstream oss;
    oss.str(""); // 清空字符串流
    oss << threadId;
    return oss.str();
}

std::string GetThreadID_4()
{
    //! 线程安全，稍稍会慢一点点，占用空间也会多一点点，不过使用简单
    std::thread::id threadId = std::this_thread::get_id();
    std::ostringstream oss;
    oss << threadId;
    return oss.str();
}


std::mutex Mutex;



void TestThreadSafe()
{
    auto fun = [](){
        for (int i = 0; i < 100; ++i) {
            auto id = GetThreadID_3();
            Mutex.lock();
//            printf("%zu\n", id);
            printf("%s\n", id.c_str());
            Mutex.unlock();
        }
    };

    std::thread T1(fun);
    std::thread T2(fun);
    std::thread T3(fun);
    std::thread T4(fun);
    std::thread T5(fun);
    std::thread T6(fun);
    std::thread T7(fun);
    std::thread T8(fun);
    std::thread T9(fun);

    T1.join();
    T2.join();
    T3.join();
    T4.join();
    T5.join();
    T6.join();
    T7.join();
    T8.join();
    T9.join();
}



int main()
{
    printf("%zu\n", GetThreadID_1());
    printf("%zu\n", GetThreadID_2());
    printf("%s\n", GetThreadID_3().c_str());
    printf("%s\n", GetThreadID_3().c_str());


    return 0;
}
