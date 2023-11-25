#include "GameManager.h"
#include "log.h"
#include "LogXmlConfig.h"
#include "ConfigManager.h"

using namespace std::chrono_literals;

int main()
{
    // setvbuf(stdout, nullptr, );
    yy::app::GameManager::getInstance().RunApp();
}

