#include "CenterServerManager.h"
#include "log.h"
#include "ConfigManager.h"

using namespace std::chrono_literals;

int main()
{
    try {
        yy::config::ConfigManager::AddFilePath("../config/configs_center.xml");
        yy::app::center::CenterServerManager::Instance().RunApp();
    } catch (const std::exception &e) {
        std::cerr << "Uncaught exception: " << e.what() << std::endl;
        return 114514;
    }

    CLOSE_YLOG();
    google::protobuf::ShutdownProtobufLibrary();
    std::cout << "---------main end---------" << std::endl;
    return 0;
}
