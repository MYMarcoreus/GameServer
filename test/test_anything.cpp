#include <iostream>
#include <functional>
#include <format>
#include <ThreadPool.h>

using namespace std;
using namespace std::placeholders;

using Task = std::function<void()>;

static void GoodTask(int i, string s)
{
    std::cout << std::format("Hello {}, {}\n", i, s);
}

void fun(Task task)
{
    task();
}


int main()
{
    int i = 10;
    string s = "game";

    /* 使用std::bind将i和s参数填入有两个参数`void GoodTask(int, string)`的函数GoodTask，
    使其成为为无参函数`void GoodTask()`，但需要std::bind作为实参需要std::funciton作为形参来接收变换后的GoodTask */
//    fun( std::bind(&GoodTask, i, s) );
//    fun([i, s](){ GoodTask(i, s); });

    yy::util::ThreadPool pool;

    pool.Start();
    for (int j = 0; j < 10; ++j) {
        pool.PushTask( [s, i](){ GoodTask(i,s); } );
    }

    sleep(5);

    return 0;
}