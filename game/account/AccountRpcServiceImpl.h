#pragma once

#include "account.pb.h"
#include "CenterRpcClient.h"
#include <google/protobuf/service.h>


namespace yy::net
{
class EventLoop;
}

namespace yy::core
{
namespace zk
{
    class ZkServiceManager;
}
namespace rpc { class RpcServer; }
class MySqlClient;
class RedisClient;
class IServer;
}

namespace yy::app::account
{

class AccountRpcServiceImpl final : public yy::protocol::app::AccountServiceRpc {
public:
    explicit AccountRpcServiceImpl(net::EventLoop * loop);

    void Login(google::protobuf::RpcController* controller,
               const ::yy::protocol::app::C2SLogin* request,
               yy::protocol::app::S2CLogin* response,
               google::protobuf::Closure* done) override;

    void Register(google::protobuf::RpcController* controller,
                  const yy::protocol::app::C2SRegister* request,
                  yy::protocol::app::S2CRegister* response,
                  google::protobuf::Closure* done) override;

private:
    yy::core::IServer & server_;
    yy::core::rpc::RpcServer & rpc_server_;
    yy::core::RedisClient& redis_client_;
    yy::core::MySqlClient& mysql_client_;
    CenterRpcClient& center_client_;
};

}
