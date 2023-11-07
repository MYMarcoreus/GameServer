#include "log.h"

using namespace yy::Ylog;

void test_log_mutex()
{
    std::vector<std::thread> my_threads;
    for (char i = 0; i < 3; i++) {
        std::thread t([]() {
            for(int j = 0 ; j < 10000 ; ++j) {
                YLOG_INFO("gogogo <%d>", j);
                YLOG_LEVEL("a", LogLevel::INFO, "nice to meet <%d>", j);
                YLOG_LEVEL("b", LogLevel::INFO, "I am fine <%d>", j);
            }
        });
        t.detach();
    }

    getchar();
}


int main() {
    yy::config::LoadConfigs();
    LoggerManager::getInstance()->getLogger("b")->Log( std::make_shared<LogMessage>(LogLevel::INFO, __FILE__, __LINE__, std::this_thread::get_id(), "fuck you!"));

    sleep(3);

}


