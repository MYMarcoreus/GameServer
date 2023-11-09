#include "log.h"
#include "ConfigManager.h"

using namespace yy::Ylog;

void test_log_mutex()
{
    std::vector<std::thread> my_threads;
    for (char i = 0; i < 1; i++) {
        std::thread t([]() {
            for(int j = 0 ; j < 10 ; ++j) {
                YLOG_INFO("gogogo <{}>", j);
            }
        });
        // t.detach();
        t.join();
    }
}


int main() {
    yy::config::ConfigManager::LoadConfigs();

    // test_log_mutex();

    YLOG_INFO("gogogo <{}>", 1);
    YLOG_INFO("gogogo <{}>", 2);
    YLOG_INFO("gogogo <{}>", "fine");

    getchar();
}


