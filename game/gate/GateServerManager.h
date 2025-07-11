#pragma once


#include "IServer.h"
#include "IGameBase.h"
#include "login.pb.h"
#include "ProtobufDispatcher.h"
#include "RpcClientPool.hpp"
#include "ThreadPool.h"

using yy::core::IServer;
using std::shared_ptr;


namespace yy::app::gate
{
class AccountRpcClient;


class GateServerManager final : public Singleton<GateServerManager> {
    SINGLETON_NECESSITY(GateServerManager)
public:
    void RunApp();

    // template<typename T>
    // void RegisterMessageCallback(typename core::CallbackT<core::UserConnectionPtr, T>::ProtobufMessageTCallback callback) {
    //     RegisterMessageCallback<T>(callback);
    // }

    IServer& GetServer() const { return *m_frontend; }
private:
    GateServerManager();
    ~GateServerManager() override;

    void Init();

    void OnFrontend_Secutiry(const core::UserConnectionPtr& userconn) ;
    void OnFrontend_Disconnect(const core::UserConnectionPtr& userconn) ;
    void OnFrontend_Message(const core::UserConnectionPtr &, const core::MessagePtr &, core::MessageType type);

    void UnkonwnCommand(const core::UserConnectionPtr &, const core::MessagePtr &);




    std::unique_ptr<yy::net::EventLoop>  m_accpetorLoop;
    std::unique_ptr<IServer> m_frontend;
    core::ProtobufDispatcher<core::UserConnectionPtr> m_dispatcher; // 处理下层(core层)分发传来的无法处理的消息
    std::unique_ptr<AccountRpcClient>   m_accountRpcClient;

    yy::net::ThreadPool m_workThreads;
};

}