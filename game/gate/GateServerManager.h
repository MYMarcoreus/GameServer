#ifndef MESSAGEDISPATCHMANAGER_H
#define MESSAGEDISPATCHMANAGER_H

#include "IServer.h"
#include "IGameBase.h"
#include "ProtobufDispatcher.h"
#include "ThreadPool.h"

using yy::core::IServer;
using std::shared_ptr;


namespace yy::app::gate
{

class GateServerManager final : public Singleton<GateServerManager> {
    SINGLETON_NECESSITY(GateServerManager)
public:
    void RunApp();

    // template<typename T>
    // void RegisterMessageCallback(typename core::CallbackT<core::UserConnectionPtr, T>::ProtobufMessageTCallback callback) {
    //     RegisterMessageCallback<T>(callback);
    // }

    IServer * GetServer() const { return m_server; }
private:
    GateServerManager();
    ~GateServerManager() override;

    void Init();

    void StartListenAndIOLoop();

    void AppNotifier_Secutiry(const core::UserConnectionPtr& userdata) ;
    void AppNotifier_Disconnect(const core::UserConnectionPtr& userdata) ;
    void AppNotifier_Command(const core::UserConnectionPtr &, const core::MessagePtr &);

    void UnkonwnCommand(const core::UserConnectionPtr &, const core::MessagePtr &);




    IServer   * m_server;
    core::ProtobufDispatcher<core::UserConnectionPtr> m_dispatcher; // 处理下层(core层)分发传来的无法处理的消息
    yy::net::EventLoop * m_accpetorLoop;

    yy::net::ThreadPool m_wordThreads;
};

}

#endif //MESSAGEDISPATCHMANAGER_H
