#include "ThreadPool.h"
#include <iostream>

using yy::util::ThreadPool;

void fun()
{
    std::cout << "thread<" << std::this_thread::get_id() << ">: running!\n";
    sleep(1);
}


int main()
{
    setbuf(stdout, nullptr);

    ThreadPool pool{4};
    pool.Start();
    for (int i = 0; i < 100; ++i) {
        pool.PushTask(fun);
    }

    return 0;
}