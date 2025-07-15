#include "CenterRpcServiceImpl.h"

#include "CenterServerManager.h"
#include "RedisClient.h"
#include "ZkServiceManager.h"
#include "MySqlClient.h"
#include "RpcServer.h"

namespace yy::app::center
{
CenterRpcServiceImpl::CenterRpcServiceImpl(net::EventLoop * loop):
    rpc_server_(CenterServerManager::Instance().GetRpcServer()),
    redis_client_(core::RedisClient::Instance()),
    mysql_pool_(core::MySqlClient::Instance()),
    zk_service_(std::make_unique<core::zk::ZkServiceManager>())
{
    // 初始化Redis
    redis_client_.Start(loop, 5);
    // 初始化MySql
    mysql_pool_.Start(loop, "gameserver");
    // 初始化ZkClient
    zk_service_->Start("/services");
}

void CenterRpcServiceImpl::SelectServer(google::protobuf::RpcController* controller,
    const yy::protocol::app::SelectServerReq* request, yy::protocol::app::SelectServerRsp* response,
    google::protobuf::Closure* done)
{
    YLOG_INFO("正在执行 CenterRpcServiceImpl::SelectServer 服务，填充响应体")

    auto token = GetToken();
    auto [ip, port] = GetLogicServerAddr();

    response->set_session_id(request->session_id());
    response->set_result_code(protocol::app::SelectServerRsp_Status_eSuccess);
    response->set_username(request->username());

    response->set_ip(ip);
    response->set_port(port);
    response->set_token(token);

    done->Run();
}

auto CenterRpcServiceImpl::GetToken() -> std::string
{
    return util::GenerateToken(32);
}

auto CenterRpcServiceImpl::GetLogicServerAddr() -> std::pair<std::string, uint16_t>
{
    static std::atomic<size_t> rr_idx = 0;
    const auto endpoints = zk_service_->FetchLocalCache("RoomService");
    if (endpoints.size() > 0) {
        //todo 按照每个服务器承载的房间数量选择服务器
        const auto endpoint = endpoints[rr_idx.fetch_add(1, std::memory_order_acq_rel) % endpoints.size()];
        return std::make_pair(endpoint->GetIPStr(), endpoint->GetPort());
    } else {
        return std::make_pair("", 0);
    }
}
}
