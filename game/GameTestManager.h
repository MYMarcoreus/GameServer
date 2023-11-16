#ifndef ____GAMETESTMANAGER_H
#define ____GAMETESTMANAGER_H

#include "IGameBase.h"
#include "Singleton.h"

namespace yy::app {

class GameTestManager final: public Singleton<GameTestManager>
{
    SINGLETON_NECESSITY(GameTestManager)
public:
    void Init() ;

    // void StartListenAndIOLoop() override;

private:
    ~GameTestManager() override;
    GameTestManager();
};

}


#endif //____GAMETESTMANAGER_H
