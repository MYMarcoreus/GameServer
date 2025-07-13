#pragma once



#include "center.pb.h"
#include <google/protobuf/service.h>


namespace yy::core
{
namespace zk
{
    class ZkServiceManager;
}

class MySqlClient;
class RedisClient;
}

namespace yy::app::center
{

class CenterRpcServiceImpl final : public yy::protocol::app::CenterServiceRpc {
public:
    CenterRpcServiceImpl();

    void SelectServer(google::protobuf::RpcController* controller, const yy::protocol::app::C2SSelectServer* request,
        yy::protocol::app::S2CSelectServer* response, google::protobuf::Closure* done) override;

private:
    auto GetToken() -> std::string;
    auto GetLogicServerAddr() -> std::pair<std::string, uint16_t>;

    yy::core::RedisClient& redis_client_;
    yy::core::MySqlClient& mysql_pool_;
    yy::core::zk::ZkServiceManager & zk_;
};

}
