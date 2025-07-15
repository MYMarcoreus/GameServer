#pragma once

#include "center.pb.h"
#include <google/protobuf/service.h>


namespace yy::net
{
class EventLoop;
}

namespace yy::core {
namespace rpc
{
class RpcServer;
}
class IServer;
class MySqlClient;
class RedisClient;
namespace zk {
    class ZkServiceManager;
}
}

namespace yy::app::center
{

class CenterRpcServiceImpl final : public yy::protocol::app::CenterServiceRpc {
public:
    explicit CenterRpcServiceImpl(net::EventLoop * loop);

    void SelectServer(google::protobuf::RpcController* controller, const yy::protocol::app::SelectServerReq* request,
        yy::protocol::app::SelectServerRsp* response, google::protobuf::Closure* done) override;

private:
    auto GetToken() -> std::string;
    auto GetLogicServerAddr() -> std::pair<std::string, uint16_t>;

    yy::core::rpc::RpcServer & rpc_server_;
    yy::core::RedisClient& redis_client_;
    yy::core::MySqlClient& mysql_pool_;
    std::unique_ptr<yy::core::zk::ZkServiceManager> zk_service_;
};

}
