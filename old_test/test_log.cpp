#include "log.h"
#include "ConfigManager.h"

#include <google/protobuf/message.h>
#include "connection.pb.h"

using namespace yy::Ylog;

void test_log_mutex()
{
    std::vector<std::thread> my_threads;
    for (char i = 0; i < 6; i++) {
        std::thread t([]() {
            for(int j = 0 ; j < 100 ; ++j) {
                YLOG_INFO("gogogo <{}>", j);
            }
        });
        t.detach();
        // t.join();
    }
}


int main() {
    yy::config::ConfigManager::LoadConfigs();
    test_log_mutex();

    yy::protocol::core::XorBody xorBody;
    xorBody.set_xor_code(4096);

    auto str = xorBody.SerializeAsString();
    xorBody.ParseFromString(str);

    std::cout << (int)xorBody.xor_code() << '\n';


    getchar();
}


