#include "GameManager.h"
#include "log.h"
#include "ConfigManager.h"

using namespace std::chrono_literals;

int main()
{
    try {
        yy::config::ConfigManager::AddFilePath("../config/configs_logic.xml");
        yy::app::GameManager::getInstance().RunApp();
    } catch (const std::exception &e) {
        std::cerr << "Uncaught exception: " << e.what() << std::endl;
        return 114514;
    }

    return 0;
}
