#include "EventLoop.h"
#include "EventLoopThread.h"

#include <stdio.h>
#include <unistd.h>

#include "ConfigManager.h"
#include "log.h"
#include "util_functions.h"

using namespace yy;
using namespace yy::net;

int cnt = 0;
EventLoop* g_loop;

void printTid()
{
    printf("pid = %d, tid = %lu\n", getpid(), util::GetHashThreadID());
    printf("now %s\n", Timestamp::Now().ToString().c_str());
}

void print(const char* msg)
{
    printf("msg %s %s\n", Timestamp::Now().ToString().c_str(), msg);
    if (++cnt == 20)
    {
        g_loop->QuitLoop();
    }
}

void cancel(TimerID timer)
{
    g_loop->CancelTimer(timer);
    printf("cancelled at %s\n", Timestamp::Now().ToString().c_str());
}

int main()
{
    yy::config::ConfigManager::AddFilePath("../config/configs_logic.xml");
    yy::config::ConfigManager::AddFilePath("../../config/configs_logic.xml");
    yy::config::ConfigManager::LoadXmlConfigs();
    yy::Ylog::LoggerManager::Instance().ReadConfigs();

    printTid();
    sleep(1);
    {
        EventLoop loop(500ms);
        g_loop = &loop;

        print("main");
        loop.RunAfter(1s, std::bind(print, "once1"));
        loop.RunAfter(1500ms, std::bind(print, "once1.5"));
        loop.RunAfter(2500ms, std::bind(print, "once2.5"));
        loop.RunAfter(3500ms, std::bind(print, "once3.5"));
        TimerID t45 = loop.RunAfter(4500ms, std::bind(print, "once4.5"));
        loop.RunAfter(4200ms, std::bind(cancel, t45));
        loop.RunAfter(4800ms, std::bind(cancel, t45));
        loop.RunEvery(2s, std::bind(print, "every2"));
        TimerID t3 = loop.RunEvery(3s, std::bind(print, "every3"));
        loop.RunAfter(9001ms, std::bind(cancel, t3));

        loop.Loop();
        print("main loop exits");
    }
    sleep(1);
    {
        EventLoopThread loopThread({}, 500ms);
        EventLoop* loop = loopThread.CreateLoop();
        loop->RunAfter(2s, printTid);
        sleep(3);
        print("thread loop exits");
    }
}
