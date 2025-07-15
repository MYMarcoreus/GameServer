#pragma once

#include "IServer.h"
#include "ProtobufDispatcher.h"
#include "ThreadPool.h"
#include "ZkServiceManager.h"

using yy::core::IServer;
using std::shared_ptr;


// 业务层
namespace yy::app::logic {

class RoomService;
class TestService;




class LogicServerManager final : public Singleton<LogicServerManager>
{
    SINGLETON_NECESSITY(LogicServerManager)
public:
    void RunApp();

    template<typename T>
    void RegisterMessageCallback(typename core::CallbackT<core::UserConnectionPtr, T>::ProtobufMessageTCallback callback) {
        m_dispatcher.RegisterMessageCallback<T>(callback);
    }

    IServer& GetServer() const { return *m_server; }
private:
    LogicServerManager();
    ~LogicServerManager() override;

    void Init();

    void AppNotifier_Secutiry(const core::UserConnectionPtr& userdata) ;
    void AppNotifier_Disconnect(const core::UserConnectionPtr& userdata) ;
    void AppNotifier_Command(const core::UserConnectionPtr &, const core::MessagePtr &, const core::MessageType);

    void UnkonwnCommand(const core::UserConnectionPtr &, const core::MessagePtr &);


    std::unique_ptr<net::EventLoop>  m_accpetorLoop{};
    std::unique_ptr<IServer> m_server{};
    std::unique_ptr<RoomService> m_room_service{};
    std::unique_ptr<TestService> m_test_service{};
    core::zk::ZkServiceManager m_zk{};
    core::ProtobufDispatcher<core::UserConnectionPtr> m_dispatcher; // 处理下层(core层)分发传来的无法处理的消息
    net::ThreadPool m_workThreads;
};


}

