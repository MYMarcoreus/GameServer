#include "GateServerManager.h"
#include "log.h"
#include "EventLoop.h"
#include "GateServer.h"
#include "room.pb.h"
#include "future"
#include "IPAddress.h"
#include "AccountRpcClient.h"
#include "ThreadPool.h"
#include "RpcControllerImpl.h"

#include <functional>

using namespace std::chrono_literals;
using namespace yy::core;
using namespace yy::util;
using yy::net::TcpConnectionPtr;

using yy::protocol::app::C2SLogin;
using yy::protocol::app::C2SRegister;
using yy::protocol::app::S2CLogin;
using yy::protocol::app::S2CRegister;


template<class T>
using Ptr = std::shared_ptr<T>;


namespace yy::app::gate {


GateServerManager::GateServerManager():
    m_accpetorLoop{std::make_unique<EventLoop>(500ms)},
    m_dispatcher{[this](const UserConnectionPtr& userconn, const MessagePtr& message) { this->UnkonwnCommand(userconn, message); }},
    m_workThreads(std::make_unique<ThreadPool>("Gate Work Thread"))
{
    RegisterRpcForward<C2SLogin, S2CLogin>();
    RegisterRpcForward<C2SRegister, S2CRegister>();
}

GateServerManager::~GateServerManager() {
    m_frontend->Stop();
}


void GateServerManager::OnFrontend_Secutiry(const UserConnectionPtr& userconn) {
    userconn->SetState(UserConnection::E_UserBaseState::eSecure);
}

void GateServerManager::OnFrontend_Disconnect(const UserConnectionPtr& userconn) {
    YLOG_INFO("↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓ 用户<{}>断开连接", userconn->GetUID())
}

void GateServerManager::OnFrontend_Message(const UserConnectionPtr & userconn, const MessagePtr & message, const MessageType type)
{
    m_workThreads->PushTask([this, userconn, message](){
        m_dispatcher.OnProtobufMessage(userconn, message);
    });
}

void GateServerManager::UnkonwnCommand(const UserConnectionPtr & userconn, const MessagePtr & message)
{
    YLOG_DEBUG("未知的消息类型：{}", message->GetDescriptor()->full_name())
    userconn->Shutdown();
}





void GateServerManager::RunApp()
{
    //! 初始化服务器
    Init();

    //! 启动服务器的工作线程
    m_workThreads->Start(m_accpetorLoop.get(), config::g_app_config->GetValue().work_thread_num());

    //! 启动监听线程(即主线程)的
    m_accpetorLoop->Loop();
}

// 测试：正确登录
void GateServerManager::TestLogin1()
{
    auto req = std::make_shared<C2SLogin>();
    req->set_username("test_yy_name");
    req->set_password("114514");
    req->set_session_id(101010);
    m_accountRpcClient->CallRemoteAsync<C2SLogin, S2CLogin>(req,
    [](std::unique_ptr<S2CLogin> && response, std::unique_ptr<rpc::RpcControllerImpl> && controller) {
        switch (response->result_code()) {
        case protocol::app::S2CLogin_Status_eSuccess:
            YLOG_INFO("{} 登录成功！token为 {}, ip:{}, port:{}", response->username(), response->token(), response->ip(), response->port())
            break;
        case protocol::app::S2CLogin_Status_eAccountNotExist:
            YLOG_INFO("{} 登录失败：账号不存在", response->username())
            break;
        case protocol::app::S2CLogin_Status_ePasswordError:
            YLOG_INFO("{} 登录失败：密码错误", response->username())
            break;
        case protocol::app::S2CLogin_Status_eUnknownError:
            YLOG_INFO("{} 登录失败：未知错误", response->username())
            break;
        default: ;
        }
    });
}

// 测试：账号错误
void GateServerManager::TestLogin2()
{
    auto req = std::make_shared<C2SLogin>();
    req->set_username("not_exist_name");
    req->set_password("114514");
    req->set_session_id(101010);
    m_accountRpcClient->CallRemoteAsync<C2SLogin, S2CLogin>(req,
    [](std::unique_ptr<S2CLogin> && response, std::unique_ptr<rpc::RpcControllerImpl> && controller) {
        switch (response->result_code()) {
        case protocol::app::S2CLogin_Status_eSuccess:
            YLOG_INFO("{} 登录成功！", response->username())
            break;
        case protocol::app::S2CLogin_Status_eAccountNotExist:
            YLOG_INFO("{} 登录失败：账号不存在", response->username())
            break;
        case protocol::app::S2CLogin_Status_ePasswordError:
            YLOG_INFO("{} 登录失败：密码错误", response->username())
            break;
        case protocol::app::S2CLogin_Status_eUnknownError:
            YLOG_INFO("{} 登录失败：未知错误", response->username())
            break;
        default: ;
        }
    });
}

// 测试：密码错误
void GateServerManager::TestLogin3()
{
    auto req = std::make_shared<C2SLogin>();
    req->set_username("test_yy_name");
    req->set_password("error_pwd");
    req->set_session_id(101010);
    m_accountRpcClient->CallRemoteAsync<C2SLogin, S2CLogin>(req,
    [](std::unique_ptr<S2CLogin> && response, std::unique_ptr<rpc::RpcControllerImpl> && controller) {
        switch (response->result_code()) {
        case protocol::app::S2CLogin_Status_eSuccess:
            YLOG_INFO("{} 登录成功！", response->username())
            break;
        case protocol::app::S2CLogin_Status_eAccountNotExist:
            YLOG_INFO("{} 登录失败：账号不存在", response->username())
            break;
        case protocol::app::S2CLogin_Status_ePasswordError:
            YLOG_INFO("{} 登录失败：密码错误", response->username())
            break;
        case protocol::app::S2CLogin_Status_eUnknownError:
            YLOG_INFO("{} 登录失败：未知错误", response->username())
            break;
        default: ;
        }
    });
}

void GateServerManager::TestRegister1()
{
    auto req = std::make_shared<C2SRegister>();
    req->set_session_id(101010);
    req->set_username("test_yy_name");
    req->set_password("114514");
    m_accountRpcClient->CallRemoteAsync<C2SRegister, S2CRegister>(req,
    [](std::unique_ptr<S2CRegister> && response, std::unique_ptr<rpc::RpcControllerImpl> && controller) {
        switch (response->result_code()) {
        case protocol::app::S2CRegister_Status_eSuccess:
            YLOG_INFO("{} 注册成功！", response->session_id())
            break;
        case protocol::app::S2CRegister_Status_eAccountAlreadyExist:
            YLOG_INFO("{} 注册失败：账号已存在", response->session_id())
            break;
        case protocol::app::S2CLogin_Status_eUnknownError:
            YLOG_INFO("{} 注册失败：未知错误", response->session_id())
            break;
        default: ;
        }
    });
}



void GateServerManager::Init()
{
    //! 读取配置文件
    config::ConfigManager::LoadXmlConfigs();

    //! 读取日志配置
    Ylog::LoggerManager::Instance().ReadConfigs();

    m_accountRpcClient = std::make_unique<AccountRpcClient>();
    m_accountRpcClient->Start(3,
        [this](const TcpConnectionPtr & conn) {
            YLOG_INFO("连接至AccountRpc服务器<{}:{}>，我方地址为<{}:{}>", conn->GetPeerAddr()->GetIPStr().c_str(), conn->GetPeerAddr()->GetPort()
                                            , conn->GetLocalAddr()->GetIPStr().c_str(), conn->GetLocalAddr()->GetPort());
        }
    );

    // m_accpetorLoop->RunEvery(10ms, [this]() {
    //     this->TestRegister1();
    //     this->TestLogin1();
    //     this->TestLogin2();
    //     this->TestLogin3();
    // });


    /*********** 启动前端 ***********/
    //! 初始化监听的端口和IP地址(IP地址未给出，则使用INADDR_ANY绑定所有IP地址)
    IPAddressPtr listenAddr = std::make_shared<IPv4Address>(
        "192.168.147.128",
        config::g_app_config->GetValue().app_tcp_port()
    );

    //! 初始化服务器对象
    m_frontend = std::make_unique<GateServer>(m_accpetorLoop.get(), listenAddr);
    m_frontend->SetNotifier_Security(
        [this](const UserConnectionPtr& userconn) {
            this->OnFrontend_Secutiry(userconn);
        });
    m_frontend->SetNotifier_DisConnect(
        [this](const UserConnectionPtr & userconn) {
            this->OnFrontend_Disconnect(userconn);
        });
    m_frontend->SetNotifier_Command(
        [this](const UserConnectionPtr & userconn, const MessagePtr & message, const MessageType type) {
            this->OnFrontend_Message(userconn, message, type);
        });

    //! 启动服务器的监听和IO线程
    m_frontend->Start();
}



}
