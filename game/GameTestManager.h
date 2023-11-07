#ifndef ____GAMETESTMANAGER_H
#define ____GAMETESTMANAGER_H

#include "IGameBase.h"
#include "Singleton.h"

namespace yy::app {

class GameTestManager final: public IGameBase, public Singleton<GameTestManager>
{
    SINGLETON_NECESSITY(GameTestManager)
public:
    void Init() override;

    void Update() override;

    void AppCommand(const core::UserBaseData::ptr &, int32_t) override ;
private:
    ~GameTestManager() override;
    GameTestManager();
};

}


#endif //____GAMETESTMANAGER_H
