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
namespace redis { class RedisClient; }
namespace mysql { class MySqlClient; }
class IServer;
}

namespace yy::app::account
{

class AccountRpcServiceImpl final : public protocol::app::AccountServiceRpc {
    constexpr static std::string PWD_field = "password";
    constexpr static std::string UID_field = "uid";

public:
    explicit AccountRpcServiceImpl(net::EventLoop * loop);

    void Login(google::protobuf::RpcController* controller,
               const protocol::app::C2SLogin* request,
               protocol::app::S2CLogin* response,
               google::protobuf::Closure* done) override;

    void Register(google::protobuf::RpcController* controller,
                  const protocol::app::C2SRegister* request,
                  protocol::app::S2CRegister* response,
                  google::protobuf::Closure* done) override;

private:
    core::IServer & server_;
    core::rpc::RpcServer & rpc_server_;
    core::redis::RedisClient& redis_client_;
    core::mysql::MySqlClient& mysql_client_;
    CenterRpcClient& center_client_;
};

}
