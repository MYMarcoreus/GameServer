#pragma once
#include "AccountRpcClient.h"
#include "CenterRpcClient.h"
#include "ProtobufDispatcher.h"
#include "core_definations.h"
#include "EventLoop.h"
#include "GameData.h"
#include "GateRedisDAO.h"
#include "UnorderedMapInLoop.hpp"
#include "UserConnection.h"


namespace yy::core::rpc
{
class RpcServer;
}

namespace yy::net
{
class EventLoop;
}



namespace yy::app::gate
{
class GateRedisDAO;

class ForwardManager {
public:
    explicit ForwardManager(net::EventLoop * base_loop);

    void Start();

    //Region 登录状态相关
    ///@brief 账号服的登录响应
    bool OnBackend_LoginRsp(const UserConnectionPtr & userconn, const protocol::app::LoginRsp & response);
    ///@brief 前端连接断开，通告中心服
    void OnFrontend_Disconnect(const UserConnectionPtr & userconn);
    ///@brief 将后端响应广播至指定用户
    void BroadcastToFrontend(const protocol::app::BroadcastRoomReq & msg);
    ///@brief 前端主动退出登录
    void OnFrontend_QuitLoginReq(const UserConnectionPtr& userconn, const Ptr<protocol::app::QuitLoginReq> & req);
    ///@brief 处理用户下线（主动退出登录/连接关闭））
    void HandleQuitLogin(const UserConnectionPtr& userconn);
    //End

    ///@brief 转发已注册的前端消息到后端
    void ForwardToBackend(const UserConnectionPtr & userconn, const MessagePtr & request, core::MessageNetType type);

private:
    ///@brief 注册要转发到后端的前端消息
    template<core::IsProtobufMessage Request, core::IsProtobufMessage Response, core::rpc::IsValidStub ServiceStub>
    void RegisterRpcForward(core::rpc::RpcClient<ServiceStub> & rpcClient,
                            std::function<bool(const UserConnectionPtr &, const Request &)> onReqCb,
                            std::function<bool(const UserConnectionPtr &, const Response &)> onRspCb);

    ///@brief 过滤前端的消息
    template<core::IsProtobufMessage Request> requires core::HasTokenMethod<Request>
    bool FilterMessage(const UserConnectionPtr &, const Request &);

    ///@brief 主动发送请求，用于通知其它服务器
    template<core::IsProtobufMessage Request, core::IsProtobufMessage Response, core::rpc::IsValidStub ServiceStub>
    void SendRpcRequest(core::rpc::RpcClient<ServiceStub> & rpcClient, const Request & request,
        std::function<void(const Response &)> onRspCb);

    ///@brief 注册网关本地处理的消息（无需转发）
    template <core::IsProtobufMessage MsgT, typename ClassT> requires core::MessageHandlerInvocable<ClassT, MsgT>
    void RegisterHandler(ClassT* self, core::ProtobufDispatcher<UserConnectionPtr>& dispatcher,
        void(ClassT::*handler)(const UserConnectionPtr&, const std::shared_ptr<MsgT>&));

private:
    net::EventLoop * m_baseLoop;
    core::ProtobufDispatcher<UserConnectionPtr> m_dispatcher; // 处理下层(core层)分发传来的无法处理的消息
    GateRedisDAO& m_redisDAO; // 用于过滤前端消息：验证token

    // 后端连接：用于转发RPC消息
    rpc_client::AccountRpcClient&   m_accountRpcClient;
    rpc_client::CenterRpcClient&    m_centerRpcClient;

    // 前端连接：记录已登录的用户连接，用于广播消息
    net::UnorderedMapInLoop<UID_t, UserConnectionPtr> m_uid_to_user;
};




template<core::IsProtobufMessage Request, core::IsProtobufMessage Response, core::rpc::IsValidStub ServiceStub>
void ForwardManager::RegisterRpcForward(core::rpc::RpcClient<ServiceStub> & rpcClient,
    std::function<bool(const UserConnectionPtr &, const Request &)> onReqCb,
    std::function<bool(const UserConnectionPtr &, const Response &)> onRspCb)
{
    m_dispatcher.RegisterMessageCallback<Request>(
        //! 【收到客户端请求】
        [this, &rpcClient, onReqCb = std::move(onReqCb), onRspCb = std::move(onRspCb)]
        (const UserConnectionPtr& userconn, const std::shared_ptr<Request>& request)
        {
            if (!userconn or !request) {
                YLOG_INFO("RPC响应时：连接失效 或 {}请求为空", Request::descriptor()->name());
                return;
            }

            //! 《转发Rpc请求前过滤》
            if (onReqCb and !onReqCb(userconn, *request)) {
                YLOG_INFO("{}请求无法转发：过滤", Request::descriptor()->name());
                return;
            }

            //! 【转发RPC请求】
            YLOG_INFO("RPC转发：{}发送：{}", Request::descriptor()->name(), request->ShortDebugString());
            const bool success = rpcClient.template CallRemoteAsync_Random<Request, Response>(
                *request,
                //! 【Rpc响应回调】
                [userconn, onRspCb = std::move(onRspCb)](std::unique_ptr<Response>&& response, std::unique_ptr<core::rpc::RpcControllerImpl>&& controller)
                {
                    // 收到异步响应时玩家可能
                    if (!userconn->IsConnected() or !response) {
                        YLOG_INFO("RPC响应时：连接失效 或 {}响应为空", Request::descriptor()->name());
                        return;
                    }
                    if (!controller or controller->Failed()) {
                        YLOG_INFO("{}失败：{}", Request::descriptor()->name(), controller->ErrorText());
                        return;
                    }

                    //! 《转发Rpc响应前过滤》
                    const bool shouldSend = !onRspCb || onRspCb(userconn, *response);
                    if (shouldSend) {
                        userconn->SendTCP(*response);
                        YLOG_INFO("RPC转发：{}返回：{}", Response::descriptor()->name(), response->ShortDebugString());
                    } else {
                        YLOG_INFO("{}响应无法转发：过滤", Request::descriptor()->name());
                    }
                });

            //! RPC请求是否转发成功
            if (not success) {
                YLOG_WARN("{}转发失败", Request::descriptor()->name());
                userconn->Shutdown();
            }
        });
}

template<core::IsProtobufMessage Request> requires core::HasTokenMethod<Request>
bool ForwardManager::FilterMessage(const UserConnectionPtr& userconn, const Request& request)
{
    if (!userconn) return false;

    std::string token;
    if constexpr (core::HasToken<Request>) {
        token = request.token();
    } else
    if constexpr (core::HasUserToken<Request>) {
        token = request.user_token();
    } else {
        static_assert(core::HasToken<Request> || core::HasUserToken<Request>, "Request must have token() or user_token()");
    }

    return token == userconn->GetToken() and token == m_redisDAO.GetTokenAndRefreshEx(userconn->GetUID());
}

template <core::IsProtobufMessage Request, core::IsProtobufMessage Response, core::rpc::IsValidStub ServiceStub>
void ForwardManager::SendRpcRequest(core::rpc::RpcClient<ServiceStub>& rpcClient, const Request & request,
                                    std::function<void(const Response&)> onRspCb)
{
    //! 【转发RPC请求】
    YLOG_INFO("RPC转发：{}发送：{}", Request::descriptor()->name(), request.ShortDebugString());
    const bool success = rpcClient.template CallRemoteAsync_Random<Request, Response>(request,
        //! 【Rpc响应回调】
        [onRspCb = std::move(onRspCb)](std::unique_ptr<Response>&& response, std::unique_ptr<core::rpc::RpcControllerImpl>&& controller)
        {
            if (!response) {
                YLOG_INFO("RPC响应时：{}响应为空", Request::descriptor()->name());
                return;
            }
            if (!controller or controller->Failed()) {
                YLOG_INFO("{}失败：{}", Request::descriptor()->name(), controller->ErrorText());
                return;
            }

            if (onRspCb) {
                onRspCb(*response);
            }
        });

    //! RPC请求是否转发成功
    if (not success) {
        YLOG_WARN("{}请求发送失败", Request::descriptor()->name());
    }
}

template <core::IsProtobufMessage MsgT, typename ClassT> requires core::MessageHandlerInvocable<ClassT, MsgT>
void ForwardManager::RegisterHandler(ClassT* self, core::ProtobufDispatcher<UserConnectionPtr>& dispatcher,
    void(ClassT::* handler)(const UserConnectionPtr&, const std::shared_ptr<MsgT>&))
{
    dispatcher.RegisterMessageCallback<MsgT>(
        [this, handler](const UserConnectionPtr& user, const std::shared_ptr<MsgT>& msg) {
            (this->*handler)(user, msg);
        }
    );
}
}
