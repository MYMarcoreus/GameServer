#ifndef ____GAMEMANAGER_H
#define ____GAMEMANAGER_H

#include "IServer.h"
#include "GameData.h"
#include "IGameBase.h"
#include "codec/ProtobufDispatcher.h"
#include "ThreadPool.h"

using yy::core::IServer;
using std::shared_ptr;



// 业务层
namespace yy::app {

class GamePlayerManager;
class GameTestManager;




class GameManager : public Singleton<GameManager>
{
    SINGLETON_NECESSITY(GameManager)
public:
    void RunApp();

    template<typename T>
    void RegisterMessageCallback( core::CallbackT<T, core::UserBaseDataPtr>::ProtobufMessageTCallback callback) {
        m_dispatcher.RegisterMessageCallback<T>(callback);
    }

    IServer * GetServer() { return m_server; }
private:
    GameManager();
    ~GameManager();

    void Init();

    void StartListenAndIOLoop();

    void AppNotifier_Secutiry(const yy::net::TcpConnectionPtr& conn) ;

    void AppNotifier_Disconnect(const yy::net::TcpConnectionPtr& conn) ;

    void AppNotifier_Command(const core::UserBaseDataPtr &, const core::MessagePtr &);

    void UnkonwnCommand(const core::UserBaseDataPtr &, const core::MessagePtr &);




    IServer   * m_server;
    GamePlayerManager * m_player;
    GameTestManager   * m_test;
    core::ProtobufDispatcher<core::UserBaseDataPtr> m_dispatcher;
    yy::net::EventLoop * m_loop;

    yy::util::ThreadPool m_threadPool;
};


}

#endif //____GAMEMANAGER_H
