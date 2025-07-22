#pragma once

#include "ZkServiceManager.h"
#include "MySqlClient.h"
#include "RpcServer.h"
#include "RedisClient.h"
#include "center.pb.h"


namespace yy::net
{
class EventLoop;
}

namespace yy::app::center
{

class CenterRpcServiceImpl final : public protocol::app::CenterServiceRpc {
public:
    explicit CenterRpcServiceImpl(net::EventLoop * loop);

    void SelectServer(google::protobuf::RpcController* controller, const protocol::app::SelectServerReq* request,
        protocol::app::SelectServerRsp* response, google::protobuf::Closure* done) override;

private:
    auto GetToken() -> std::string;
    auto GetLogicServerAddr() -> std::pair<std::string, uint16_t>;

    core::rpc::RpcServer&                       rpc_server_;
    core::redis::RedisClient&                          redis_client_;
    core::mysql::MySqlClient&                          mysql_pool_;
    std::unique_ptr<core::zk::ZkServiceManager> zk_service_;
};

}
