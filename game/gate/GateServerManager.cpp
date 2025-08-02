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
#include "GateRedisDAO.h"

#include <functional>

using namespace std::chrono_literals;
using namespace yy::core;
using namespace yy::util;
using yy::net::TcpConnectionPtr;



template<class T>
using Ptr = std::shared_ptr<T>;
using namespace yy::protocol::app;


namespace yy::app::gate {


GateServerManager::GateServerManager():
    m_accpetorLoop{std::make_unique<EventLoop>(500ms)},
    m_dispatcher{[this](const UserConnectionPtr& userconn, const MessagePtr& message) { this->UnkonwnCommand(userconn, message); }},
    m_workThreads(std::make_unique<ThreadPool>("Gate Work Thread")),
    m_redisDAO(GateRedisDAO::Instance())
{
    m_accountRpcClient = std::make_unique<rpc_client::AccountRpcClient>();
    m_centerRpcClient = std::make_unique<rpc_client::CenterRpcClient>();

    assert(m_accountRpcClient);
    // 账号服务
    RegisterRpcForward<LoginReq, LoginRsp>(*m_accountRpcClient, nullptr,
    [this](const UserConnectionPtr & userconn, const LoginRsp & response) -> bool {
            if (response.result_code() == LoginRsp_Status_eSuccess) {
                userconn->SetState(UserConnection::E_UserBaseState::eLoggedIn);
                userconn->SetUID(response.account_data().uid());
                userconn->SetToken(response.token());
            }
        return true;
        });
    RegisterRpcForward<RegisterReq, RegisterRsp>(*m_accountRpcClient, nullptr, nullptr);
    // 房间服务
    RegisterRpcForward<CreateRoomReq, CreateRoomRsp>(*m_centerRpcClient,
        [this](const UserConnectionPtr& conn, const CreateRoomReq& req) -> bool {
            return FilterMessage<CreateRoomReq>(conn, req);
        }, nullptr);
    RegisterRpcForward<JoinRoomReq, JoinRoomRsp>(*m_centerRpcClient,
        [this](const UserConnectionPtr& conn, const JoinRoomReq& req) -> bool {
            return FilterMessage<JoinRoomReq>(conn, req);
        }, nullptr);
    RegisterRpcForward<QuitRoomReq, QuitRoomRsp>(*m_centerRpcClient,
        [this](const UserConnectionPtr& conn, const QuitRoomReq& req) -> bool {
            return FilterMessage<QuitRoomReq>(conn, req);
        },  nullptr);
    RegisterRpcForward<GetEnterSceneTokenReq, GetEnterSceneTokenRsp>(*m_centerRpcClient,
        [this](const UserConnectionPtr& conn, const GetEnterSceneTokenReq& req) -> bool {
            return FilterMessage<GetEnterSceneTokenReq>(conn, req);
        }, nullptr);
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

void GateServerManager::OnFrontend_Message(const UserConnectionPtr & userconn, const MessagePtr & message, const MessageNetType type)
{
    m_workThreads->PushTask([this, userconn, message]{
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

/*
// 测试：正确登录
void GateServerManager::TestLogin1()
{
    auto req = std::make_shared<LoginReq>();
    req->set_username("test_yy_name");
    req->set_password("114514");
    m_accountRpcClient->CallRemoteAsync<LoginReq, LoginRsp>(req,
    [](std::unique_ptr<LoginRsp> && response, std::unique_ptr<rpc::RpcControllerImpl> && controller) {
        switch (response->result_code()) {
        case protocol::app::LoginRsp_Status_eSuccess:
            YLOG_INFO("{} 登录成功！token为 {}, ip:{}, port:{}", response->username(), response->token(), response->ip(), response->port())
            break;
        case protocol::app::LoginRsp_Status_eAccountNotExist:
            YLOG_INFO("{} 登录失败：账号不存在", response->username())
            break;
        case protocol::app::LoginRsp_Status_ePasswordError:
            YLOG_INFO("{} 登录失败：密码错误", response->username())
            break;
        case protocol::app::LoginRsp_Status_eUnknownError:
            YLOG_INFO("{} 登录失败：未知错误", response->username())
            break;
        default: ;
        }
    });
}

// 测试：账号错误
void GateServerManager::TestLogin2()
{
    auto req = std::make_shared<LoginReq>();
    req->set_username("not_exist_name");
    req->set_password("114514");
    m_accountRpcClient->CallRemoteAsync<LoginReq, LoginRsp>(req,
    [](std::unique_ptr<LoginRsp> && response, std::unique_ptr<rpc::RpcControllerImpl> && controller)
    {
        switch (response->result_code()) {
        case protocol::app::LoginRsp_Status_eSuccess:
            YLOG_INFO("{}:{} 登录成功！", response->account_data().username(), response->account_data().uid())
            break;
        case protocol::app::LoginRsp_Status_eAccountNotExist:
            YLOG_INFO("{}:{} 登录失败：账号不存在", response->account_data().username(), response->account_data().uid())
            break;
        case protocol::app::LoginRsp_Status_ePasswordError:
            YLOG_INFO("{}:{} 登录失败：密码错误", response->account_data().username(), response->account_data().uid())
            break;
        case protocol::app::LoginRsp_Status_eUnknownError:
            YLOG_INFO("{}:{} 登录失败：未知错误", response->account_data().username(), response->account_data().uid())
            break;
        default: ;
        }
    });
}

// 测试：密码错误
void GateServerManager::TestLogin3()
{
    auto req = std::make_shared<LoginReq>();
    req->set_username("test_yy_name");
    req->set_password("error_pwd");
    m_accountRpcClient->CallRemoteAsync<LoginReq, LoginRsp>(req,
    [](std::unique_ptr<LoginRsp> && response, std::unique_ptr<rpc::RpcControllerImpl> && controller) {
        switch (response->result_code()) {
        case protocol::app::LoginRsp_Status_eSuccess:
            YLOG_INFO("{} 登录成功！", response->username())
            break;
        case protocol::app::LoginRsp_Status_eAccountNotExist:
            YLOG_INFO("{} 登录失败：账号不存在", response->username())
            break;
        case protocol::app::LoginRsp_Status_ePasswordError:
            YLOG_INFO("{} 登录失败：密码错误", response->username())
            break;
        case protocol::app::LoginRsp_Status_eUnknownError:
            YLOG_INFO("{} 登录失败：未知错误", response->username())
            break;
        default: ;
        }
    });
}

void GateServerManager::TestRegister1()
{
    auto req = std::make_shared<RegisterReq>();
    req->set_username("test_yy_name");
    req->set_password("114514");
    m_accountRpcClient->CallRemoteAsync<RegisterReq, RegisterRsp>(req,
    [](std::unique_ptr<RegisterRsp> && response, std::unique_ptr<rpc::RpcControllerImpl> && controller) {
        switch (response->result_code()) {
        case protocol::app::RegisterRsp_Status_eSuccess:
            YLOG_INFO("{} 注册成功！", response->uid())
            break;
        case protocol::app::RegisterRsp_Status_eAccountAlreadyExist:
            YLOG_INFO("{} 注册失败：账号已存在", response->uid())
            break;
        case protocol::app::LoginRsp_Status_eUnknownError:
            YLOG_INFO("{} 注册失败：未知错误", response->uid())
            break;
        default: ;
        }
    });
}

*/


void GateServerManager::Init()
{
    //! 读取配置文件
    config::ConfigManager::LoadXmlConfigs();

    //! 读取日志配置
    Ylog::LoggerManager::Instance().ReadConfigs();

    m_accountRpcClient->Start(
        // 3,
        [this](const TcpConnectionPtr & conn) {
            YLOG_INFO("连接至AccountRpc服务器<{}:{}>，我方地址为<{}:{}>", conn->GetPeerAddr()->GetIPStr().c_str(), conn->GetPeerAddr()->GetPort()
                                            , conn->GetLocalAddr()->GetIPStr().c_str(), conn->GetLocalAddr()->GetPort());
        }
    );

    m_centerRpcClient->Start(
        // 3,
        [this](const TcpConnectionPtr & conn) {
            YLOG_INFO("连接至CenterRpc服务器<{}:{}>，我方地址为<{}:{}>", conn->GetPeerAddr()->GetIPStr().c_str(), conn->GetPeerAddr()->GetPort()
                                            , conn->GetLocalAddr()->GetIPStr().c_str(), conn->GetLocalAddr()->GetPort());
        }
    );

    m_redisDAO.Start(m_accpetorLoop.get());

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
        [this](const UserConnectionPtr & userconn, const MessagePtr & message, const MessageNetType type) {
            this->OnFrontend_Message(userconn, message, type);
        });

    //! 启动服务器的监听和IO线程
    m_frontend->Start();
}



}
