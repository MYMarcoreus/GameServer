#include "AccountServerManager.h"
#include "log.h"
#include "ConfigManager.h"

using namespace std::chrono_literals;

int main()
{
    try {
        yy::config::ConfigManager::AddFilePath(
            yy::config::ConfigManager::GetProjectRoot() / "config" / "configs_account.xml");
        yy::app::account::AccountServerManager::Instance().RunApp();
    } catch (const std::exception &e) {
        std::cerr << "Uncaught exception: " << e.what() << std::endl;
        return 114514;
    }

    CLOSE_YLOG();
    google::protobuf::ShutdownProtobufLibrary();
    std::cout << "---------main end---------" << std::endl;
    return 0;
}
