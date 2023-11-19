#include "GameManager.h"
#include "log.h"
#include "LogXmlConfig.h"
#include "ConfigManager.h"
#include <sstream>

int main()
{
    setbuf(stdout, nullptr);
    yy::app::GameManager::getInstance().RunApp();
}

