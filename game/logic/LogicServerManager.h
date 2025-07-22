#pragma once

#include "IServer.h"
#include "ProtobufDispatcher.h"
#include "ThreadPool.h"
#include "ZkServiceManager.h"


using namespace yy::net;
using namespace yy::core;

// 业务层
namespace yy::app::logic {

class RoomService;
class TestService;

class LogicServerManager final : public Singleton<LogicServerManager>
{
    SINGLETON_NECESSITY(LogicServerManager)
public:
    void RunApp();

    // 假设你的类中有这个成员函数
    template<IsProtobufMessage MsgT>
    void RegisterHandler(void (LogicServerManager::*handler)(const UserConnectionPtr&, const shared_ptr<MsgT>&))
        requires requires(LogicServerManager* self, const UserConnectionPtr& user, const shared_ptr<MsgT>& msg) {
            (self->*handler)(user, msg);
        }
    {
        this->RegisterMessageCallback<MsgT>(
            [this, handler](const UserConnectionPtr& user, const shared_ptr<MsgT>& msg) {
                (this->*handler)(user, msg);
            }
        );
    }



    template<typename T>
    void RegisterMessageCallback(typename CallbackT<UserConnectionPtr, T>::ProtobufMessageTCallback callback) {
        m_dispatcher.RegisterMessageCallback<T>(callback);
    }

    IServer& GetServer() const { return *m_server; }
private:
    LogicServerManager();
    ~LogicServerManager() override;

    void Init();

    void AppNotifier_Secutiry(const UserConnectionPtr& userdata) ;
    void AppNotifier_Disconnect(const UserConnectionPtr& userdata) ;
    void AppNotifier_Command(const UserConnectionPtr &, const MessagePtr &, MessageType);

    void UnkonwnCommand(const UserConnectionPtr &, const MessagePtr &);


    unique_ptr<EventLoop>     m_accpetorLoop{};
    unique_ptr<IServer>            m_server{};
    unique_ptr<RoomService>        m_room_service{};
    unique_ptr<TestService>        m_test_service{};
    zk::ZkServiceManager          m_zk{};
    ProtobufDispatcher<UserConnectionPtr> m_dispatcher; // 处理下层(core层)分发传来的无法处理的消息
    ThreadPool m_workThreads;
};


}

