#include<iostream>
#include<queue>
#include<unordered_map>
#include<map>
#include<memory>
#include<thread>
#include<sstream>
#include<vector>
#include<atomic>

#include "BoundedQueue.hpp"

using namespace std;



struct Message
{
    int m_level{};    // 日志级别
    std::string m_time;     // 产生日志信息的时间
    std::string m_filename; // 产生日志信息的文件名
    uint32_t m_line{};     // 产生日志信息的代码所在行号
    std::thread::id m_threadID; // 线程号
    std::string m_content;  // 具体内容
};


yy::util::BoundedQueue<Message> Q{5};
mutex m_mutex;


void test_try_push()
{
    for(int i = 0 ; i<100 ; ++i) {
        auto m = Message(i, to_string(22),
                         to_string(i), i, this_thread::get_id(), "nice to meet you");
        if(Q.try_push(m))
        {
            ::printf("thread1: push %d, %d\n", m.m_level, Q.size());
        } else {
            i--;
        }
    }
}

void test_wait_pop()
{
    for(int i = 0 ; i < 100 ; ++i) {
        this_thread::sleep_for(0.1s);

        auto p = Q.wait_pop();
        ::printf("                      thread2: pop  %d, %d \n", p->m_level, Q.size());

    }
}

void test_wait_push()
{
    for(int i = 0 ; i<100 ; ++i) {
        auto m = Message(i, to_string(22),
                         to_string(i), i, this_thread::get_id(), "nice to meet you");
        Q.wait_push(m);
        ::printf("thread1: push %d, %d\n", m.m_level, Q.size());
    }
}


int main()
{
    thread thread1(test_wait_push);
    thread thread2(test_wait_pop);

    thread1.join();
    thread2.join();

    cout << Q.size() << endl;
}