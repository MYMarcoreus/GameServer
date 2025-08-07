#include "ForwardManager.h"

#include "CodecUtils.hpp"
#include "EventLoop.h"

using namespace std::chrono_literals;
using namespace yy::core;
using namespace yy::util;
using namespace yy::net;
using yy::net::TcpConnectionPtr;

namespace yy::app::gate
{

ForwardManager::ForwardManager(EventLoop * base_loop):
    m_baseLoop{base_loop},
    m_dispatcher{[this](const UserConnectionPtr& userconn, const MessagePtr& message) {
        YLOG_DEBUG("未知的消息类型：{}", message->GetDescriptor()->full_name())
        userconn->Shutdown();
    }},
    m_redisDAO(GateRedisDAO::Instance()),
    m_accountRpcClient{rpc_client::AccountRpcClient::Instance()},
    m_centerRpcClient{rpc_client::CenterRpcClient::Instance()},
    m_uid_to_user(base_loop)
{
    // 账号服务
    RegisterRpcForward<LoginReq, LoginRsp>(m_accountRpcClient,
        nullptr,
        [this](const UserConnectionPtr & userconn, const LoginRsp & response) -> bool {
            return OnBackend_LoginRsp(userconn, response);
        });
    RegisterRpcForward<RegisterReq, RegisterRsp>(m_accountRpcClient, nullptr, nullptr);
    // 房间服务
    RegisterRpcForward<SearchRoomReq, SearchRoomRsp>(m_centerRpcClient,
        [this](const UserConnectionPtr& conn, const SearchRoomReq& req) -> bool {
            return FilterMessage<SearchRoomReq>(conn, req);
        },
        nullptr);
    RegisterRpcForward<CreateRoomReq, CreateRoomRsp>(m_centerRpcClient,
        [this](const UserConnectionPtr& conn, const CreateRoomReq& req) -> bool {
            return FilterMessage<CreateRoomReq>(conn, req);
        },
        nullptr);
    RegisterRpcForward<SelfJoinRoomReq, SelfJoinRoomRsp>(m_centerRpcClient,
        [this](const UserConnectionPtr& conn, const SelfJoinRoomReq& req) -> bool {
            return FilterMessage<SelfJoinRoomReq>(conn, req);
        },
        nullptr);
    RegisterRpcForward<SelfQuitRoomReq, SelfQuitRoomRsp>(m_centerRpcClient,
        [this](const UserConnectionPtr& conn, const SelfQuitRoomReq& req) -> bool {
            return FilterMessage<SelfQuitRoomReq>(conn, req);
        },
        nullptr);
    RegisterRpcForward<GetEnterSceneTokenReq, GetEnterSceneTokenRsp>(m_centerRpcClient,
        [this](const UserConnectionPtr& conn, const GetEnterSceneTokenReq& req) -> bool {
            return FilterMessage<GetEnterSceneTokenReq>(conn, req);
        },
        nullptr);

    RegisterHandler(this, this->m_dispatcher  , &ForwardManager::OnFrontend_QuitLoginReq);

}

void ForwardManager::Start()
{
    m_accountRpcClient.SetConnectionEstablishedCallback(
    [this](const TcpConnectionPtr & conn) {
        YLOG_INFO("连接至AccountRpc服务器<{}:{}>，我方地址为<{}:{}>", conn->GetPeerAddr()->GetIPStr().c_str(), conn->GetPeerAddr()->GetPort(),
            conn->GetLocalAddr()->GetIPStr().c_str(), conn->GetLocalAddr()->GetPort());
    }
);
    m_centerRpcClient.SetConnectionEstablishedCallback(
        [this](const TcpConnectionPtr & conn) {
            YLOG_INFO("连接至CenterRpc服务器<{}:{}>，我方地址为<{}:{}>", conn->GetPeerAddr()->GetIPStr().c_str(), conn->GetPeerAddr()->GetPort(),
                conn->GetLocalAddr()->GetIPStr().c_str(), conn->GetLocalAddr()->GetPort());
        }
    );
}

bool ForwardManager::OnBackend_LoginRsp(const UserConnectionPtr& userconn, const LoginRsp& response)
{
    if (response.result_code() == LoginRsp_Status_eSuccess) {
        userconn->SetState(UserConnection::E_UserBaseState::eLoggedIn);
        userconn->SetUID(response.account_data().uid());
        userconn->SetToken(response.token());
        m_uid_to_user.Insert(userconn->GetUID(), userconn);

        m_redisDAO.SetAccountData(response.account_data().uid(), response.account_data().username());
    }
    return true;
}

void ForwardManager::OnFrontend_Disconnect(const UserConnectionPtr& userconn)
{
    HandleQuitLogin(userconn);

    // 通告中心服删掉玩家房间数据
    UserDisconnectReq req;
    req.set_uid(userconn->GetUID());
    SendRpcRequest<UserDisconnectReq, UserDisconnectRsp>(m_centerRpcClient, req, nullptr);
}

void ForwardManager::BroadcastToFrontend(const BroadcastRoomReq& msg)
{
    MessagePtr bro_msg = CreateMessage(msg.msg_cmd());
    bro_msg->ParseFromString(msg.payload());
    for (auto target_uid: msg.target_uids())
    {
        m_uid_to_user.Get(target_uid, [bro_msg, msg](const std::optional<UserConnectionPtr>& op_userconn)
        {
            if (op_userconn.has_value()) {
                const auto & userconn = op_userconn.value();
                YLOG_INFO("广播 {} 到 {}", g_cmd_to_name[msg.msg_cmd()], userconn->GetUID())
                userconn->SendTCP(bro_msg);
            }
        });
    }
}

void ForwardManager::ForwardToBackend(const UserConnectionPtr& userconn, const MessagePtr& request, const MessageNetType)
{
    //! 直接在IO线程里转发
    m_dispatcher.OnProtobufMessage(userconn, request);
}

void ForwardManager::OnFrontend_QuitLoginReq(const UserConnectionPtr& userconn, const Ptr<QuitLoginReq>& req)
{
    HandleQuitLogin(userconn);

    // 发送响应
    QuitLoginRsp rsp;
    rsp.set_uid(req->uid());
    if (userconn->IsLoggedIn()) {
        rsp.set_result_code(QuitLoginRsp_Status_eSuccess);
    } else {
        rsp.set_result_code(QuitLoginRsp_Status_eNotLogin);
    }
    userconn->SendTCP(rsp);
}

void ForwardManager::HandleQuitLogin(const UserConnectionPtr& userconn)
{
    // 删掉token和前端映射
    const bool is_del = m_redisDAO.DelToken(userconn->GetUID());
    if (is_del) {
        YLOG_INFO("[GateServerManager::OnFrontend_Disconnect] 用户<{}>Token被删除", userconn->GetUID())
    }
    m_uid_to_user.Erase(userconn->GetUID());
}
}
