#pragma once

#include "core_definations.h"
#include "account.pb.h"
#include "AccountRpcClient.h"
#include "CenterRpcClient.h"
#include "GateRedisDAO.h"
#include "ProtobufDispatcher.h"
#include "RpcStubConnectionPool.hpp"
#include "UserConnection.h"


using namespace yy::net;
using namespace yy::core;
using std::shared_ptr;

namespace yy::app::gate
{
class GateRedisDAO;


class GateServerManager final : public Singleton<GateServerManager> {
    SINGLETON_NECESSITY(GateServerManager)
public:
    void RunApp();

    IServer& GetServer() const { return *m_frontend; }
private:
    GateServerManager();
    ~GateServerManager() override;

    void Init();

    void OnFrontend_Secutiry(const UserConnectionPtr& userconn) ;
    void OnFrontend_Disconnect(const UserConnectionPtr& userconn) ;
    void OnFrontend_Message(const UserConnectionPtr &, const MessagePtr &, MessageNetType type);

    void UnkonwnCommand(const UserConnectionPtr &, const MessagePtr &);

    template<IsProtobufMessage Request, IsProtobufMessage Response, rpc::IsValidStub ServiceStub>
    void RegisterRpcForward(rpc::RpcClient<ServiceStub> & rpcClient,
        std::function<bool(const UserConnectionPtr &, const Request &)> onRequestCallback,
        std::function<bool(const UserConnectionPtr &, const Response &)> onResponseCallback);

    template<IsProtobufMessage Request> requires HasTokenMethod<Request>
    bool FilterMessage(const UserConnectionPtr &, const Request &);

    // void TestLogin1();
    // void TestLogin2();
    // void TestLogin3();
    // void TestRegister1();

    std::unique_ptr<EventLoop>  m_accpetorLoop;
    std::unique_ptr<IServer> m_frontend;
    ProtobufDispatcher<UserConnectionPtr> m_dispatcher; // 处理下层(core层)分发传来的无法处理的消息
    std::unique_ptr<rpc_client::AccountRpcClient>   m_accountRpcClient;
    std::unique_ptr<rpc_client::CenterRpcClient>    m_centerRpcClient;
    std::unique_ptr<ThreadPool> m_workThreads;
    GateRedisDAO& m_redisDAO;
};


template<IsProtobufMessage Request, IsProtobufMessage Response, rpc::IsValidStub ServiceStub>
void GateServerManager::RegisterRpcForward(rpc::RpcClient<ServiceStub> & rpcClient,
    std::function<bool(const UserConnectionPtr &, const Request &)> onRequestCallback,
    std::function<bool(const UserConnectionPtr &, const Response &)> onResponseCallback)
{
    m_dispatcher.RegisterMessageCallback<Request>(
        [this, &rpcClient, cb_req = std::move(onRequestCallback), cb_rsp = std::move(onResponseCallback)]
        (const UserConnectionPtr& userconn, const std::shared_ptr<Request>& request)
        {
            if (request == nullptr) {
                YLOG_INFO("{}请求为空", Request::descriptor()->name());
                return;
            }

            //! 转发前处理
            if (cb_req and cb_req(userconn, *request) == false) {
                YLOG_INFO("{}请求无法转发：前处理失败", Request::descriptor()->name());
                return;
            }

            //! 转发RPC请求
            YLOG_INFO("RPC转发：{}发送：{}", Request::descriptor()->name(), request->ShortDebugString());
            const bool success = rpcClient.template CallRemoteAsync_Random<Request, Response>(
                request,
                //! 设置Rpc响应回调
                [userconn, onResponseCallback = std::move(cb_rsp)](std::unique_ptr<Response>&& response, std::unique_ptr<rpc::RpcControllerImpl>&& controller)
                {
                    if (!controller || controller->Failed()) {
                        YLOG_INFO("{}失败！", Request::descriptor()->name());
                        return;
                    }

                    if (response) {
                        //! Rpc响应前处理
                        if (onResponseCallback)
                            onResponseCallback(userconn, *response);
                        userconn->SendTCP(*response);
                        YLOG_INFO("RPC转发：{}返回：{}", Response::descriptor()->name(), response->ShortDebugString());
                    }
                });

            //! RPC请求是否转发成功
            if (not success) {
                YLOG_WARN("{}转发失败", Request::descriptor()->name());
                userconn->Shutdown();
            }
        });
}

template<IsProtobufMessage Request> requires HasTokenMethod<Request>
bool GateServerManager::FilterMessage(const UserConnectionPtr& userconn, const Request& request)
{
    if (!userconn) return false;

    std::string token;
    if constexpr (HasToken<Request>) {
        token = request.token();
    } else
    if constexpr (HasUserToken<Request>) {
        token = request.user_token();
    } else {
        static_assert(HasToken<Request> || HasUserToken<Request>, "Request must have token() or user_token()");
    }

    return token == userconn->GetToken() and token == m_redisDAO.GetTokenAndRefreshEx(userconn->GetUID());
}
}
