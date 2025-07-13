#include "LoginServerManager.h"
#include "log.h"
#include "ConfigManager.h"

using namespace std::chrono_literals;

int main()
{
    try {
        yy::config::ConfigManager::AddFilePath("../config/configs_login.xml");
        yy::app::login::LoginServerManager::Instance().RunApp();
    } catch (const std::exception &e) {
        std::cerr << "Uncaught exception: " << e.what() << std::endl;
        return 114514;
    }

    CLOSE_YLOG();
    google::protobuf::ShutdownProtobufLibrary();
    std::cout << "---------main end---------" << std::endl;
    return 0;
}
