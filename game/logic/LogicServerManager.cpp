#include "LogicServerManager.h"
#include "RoomService.h"
#include "TestService.h"
#include "log.h"
#include "EventLoop.h"
#include "LogicServer.h"
#include "future"
#include <functional>

using namespace std::chrono_literals;

namespace yy::app::logic {


LogicServerManager::LogicServerManager():
    m_dispatcher{[this](const UserConnectionPtr& userdata, const MessagePtr& message) { this->UnkonwnCommand(userdata, message); }},
    m_workThreads("Game Work Thread")
{ }

LogicServerManager::~LogicServerManager() {
    m_server->Stop();
}

void LogicServerManager::AppNotifier_Secutiry(const UserConnectionPtr& userdata) {
    userdata->SetState(UserConnection::E_UserBaseState::eSecure);
}

void LogicServerManager::AppNotifier_Disconnect(const UserConnectionPtr& userdata) {
    YLOG_INFO("↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓ 用户<{}>断开连接", userdata->GetUID())

    // 已登陆，保存数据
    if(userdata->IsLoggedIn())
    {
        //! 被动离开时执行
        YLOG_INFO("<{}> Saving Data Now!", userdata->GetSocketFD())
        m_room_service->LeaveAndSave(userdata);
        YLOG_INFO("<{}> User Data Saved!", userdata->GetSocketFD())
    }
    else // 未登录，重置数据
    {
        YLOG_INFO("<{}> DataReset", userdata->GetSocketFD())
        userdata->Shutdown();
    }
}

void LogicServerManager::AppNotifier_Command(const UserConnectionPtr & userdata, const MessagePtr & message, const MessageType type)
{
    //! 对于游戏游戏，并不在IO线程处理，而是在专门处理游戏数据的工作线程中处理（让Game层的分发器找到该游戏消息所注册的对应的处理函数。）
    m_workThreads.PushTask([this, userdata, message] { // 注意这里跨线程传输需要拷贝智能指针
        m_dispatcher.OnProtobufMessage(userdata, message);
    });
}

void LogicServerManager::UnkonwnCommand(const UserConnectionPtr & userdata, const MessagePtr & message)
{
    YLOG_DEBUG("未知的消息类型：{}", message->GetDescriptor()->full_name())
    userdata->Shutdown();
}

void LogicServerManager::RunApp()
{
    //! 初始化服务器
    Init();

    //! 启动服务器的工作线程
    m_workThreads.Start(m_accpetorLoop.get(), config::g_app_config->GetValue().work_thread_num());

    //! 启动监听线程(即主线程)的
    m_accpetorLoop->Loop();
}


void LogicServerManager::Init()
{
    //! ①、读取配置文件
    config::ConfigManager::LoadXmlConfigs();

    //! ②、读取日志配置
    Ylog::LoggerManager::Instance().ReadConfigs();

    //! ③、初始化
    m_accpetorLoop = make_unique<EventLoop>(500ms);

    //! ④、初始化监听的端口和IP地址(IP地址未给出，则使用INADDR_ANY绑定所有IP地址)
    const IPAddressPtr listenAddr = std::make_shared<IPv4Address>(
            "192.168.147.128",
            config::g_app_config->GetValue().app_tcp_port()
        );

    //! ⑤、初始化服务器对象（③和④）
    m_server = make_unique<LogicServer>(m_accpetorLoop.get(), listenAddr);
    m_server->SetNotifier_Security(
        [this](const UserConnectionPtr& userdata) {
            this->AppNotifier_Secutiry(userdata);
        });
    m_server->SetNotifier_DisConnect(
        [this](const UserConnectionPtr & userdata) {
            this->AppNotifier_Disconnect(userdata);
        });
    m_server->SetNotifier_Command(
        [this](const UserConnectionPtr & userdata, const MessagePtr & message, const MessageType type) {
            this->AppNotifier_Command(userdata, message, type);
        });

    m_room_service = make_unique<RoomService>();
    m_room_service->Init();

    m_test_service = make_unique<TestService>();
    m_test_service->Init();

    m_zk.Start("/services");
    m_zk.Register("RoomService", listenAddr->GetIPStr(), listenAddr->GetPortStr());

    //! 启动服务器的监听和IO线程
    m_server->Start();
}


}
