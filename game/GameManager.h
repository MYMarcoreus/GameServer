#ifndef ____GAMEMANAGER_H
#define ____GAMEMANAGER_H

#include "IServer.h"
#include "GameData.h"
#include "IGameBase.h"

using yy::core::IServer;
using std::shared_ptr;

// 业务层
namespace yy::app {

class GameManager
{
public:
    static void StartApp();
private:
    static void Init();
    static void Update();

    static IServer   * m_server;
    static IGameBase * m_player;
    static IGameBase * m_test;
};


}

#endif //____GAMEMANAGER_H
