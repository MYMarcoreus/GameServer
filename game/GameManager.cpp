#include "GameManager.h"

#include "GamePlayerManager.h"
#include "GameTestManager.h"
#include "GameNotifier.h"

//#include "log.h"
//#include <thread>
//#include <chrono>

using namespace std::chrono_literals;

namespace yy::app {

IServer   * GameManager::m_server = nullptr;
IGameBase * GameManager::m_player = nullptr;
IGameBase * GameManager::m_test   = nullptr;

void GameManager::StartApp()
{
    Init();

    while(m_server->isRunning())
    {
        Update();
    }
}

void GameManager::Init()
{
    m_server = &yy::core::get_server_instance();
    m_player = &GamePlayerManager::getInstance();
    m_test   = &GameTestManager::getInstance();

    m_server->setNotifier_Connect(AppNotifier_Connect);
    m_server->setNotifier_Security(AppNotifier_Secutiry);
    m_server->setNotifier_DisConnect(AppNotifier_Disconnect);
    m_server->setNotifier_Command(AppNotifier_Command);
    m_server->Start();

    m_player->Init();
    m_test->Init();
}

void GameManager::Update()
{
    m_server->Update();
    m_player->Update();
    m_test->Update();
}

}