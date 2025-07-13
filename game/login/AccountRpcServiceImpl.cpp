#include "AccountRpcServiceImpl.h"
#include "IPAddress.h"
#include "log.h"
#include "RedisClient.h"
#include "ZkServiceManager.h"
#include "MySqlClient.h"

namespace yy::app::login
{
AccountRpcServiceImpl::AccountRpcServiceImpl():
    redis_client_(core::RedisClient::Instance()),
    mysql_pool_(core::MySqlClient::Instance()),
    zk_(core::zk::ZkServiceManager::Instance())
{ }

void AccountRpcServiceImpl::Login(google::protobuf::RpcController* controller, const yy::protocol::app::C2SLogin* request,
                              yy::protocol::app::S2CLogin* response, google::protobuf::Closure* done)
{
    YLOG_TRACE("正在执行 AccountRpcServiceImpl::Login 服务，填充响应体")

    std::string token = GetToken();
    auto [ip, port] = GetLogicServerAddr();

    response->set_session_id(request->session_id());
    response->set_token(token);
    response->set_ip(ip);
    response->set_port(port);
    response->set_username(request->username());
    response->set_result_code(protocol::app::S2CLogin_Status_eSuccess);

    done->Run();
}

void AccountRpcServiceImpl::Register(google::protobuf::RpcController* controller, const yy::protocol::app::C2SRegister* request,
    yy::protocol::app::S2CRegister* response, google::protobuf::Closure* done)
{
    YLOG_TRACE("正在执行 AccountRpcServiceImpl::Register 服务，填充响应体")
    response->set_session_id(114514);
    response->set_result_code(protocol::app::S2CRegister_Status_eSuccess);

    done->Run();
}

auto AccountRpcServiceImpl::GetToken() -> std::string
{
    return util::GenerateToken(32);
}

auto AccountRpcServiceImpl::GetLogicServerAddr() -> std::pair<std::string, uint16_t>
{
    static std::atomic<size_t> rr_idx = 0;
    const auto endpoints = zk_.FetchLocalCache("RoomService");
    if (endpoints.size() > 0) {
        const auto endpoint = endpoints[rr_idx.fetch_add(1, std::memory_order_acq_rel) % endpoints.size()];
        return std::make_pair(endpoint->GetIPStr(), endpoint->GetPort());
    } else {
        return std::make_pair("", 0);
    }
}
}
