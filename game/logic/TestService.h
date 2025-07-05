#ifndef ____GAMETESTMANAGER_H
#define ____GAMETESTMANAGER_H

#include "IGameBase.h"
#include "Singleton.h"

namespace yy::app::logic {

class TestService final: public Singleton<TestService>
{
    SINGLETON_NECESSITY(TestService)
public:
    void Init() ;

    // void StartListenAndIOLoop() override;

private:
    ~TestService() override;
    TestService();
};

}


#endif //____GAMETESTMANAGER_H

