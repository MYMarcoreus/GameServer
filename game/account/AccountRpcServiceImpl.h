#pragma once

#include "account.pb.h"
#include "CenterRpcClient.h"

namespace yy::net { class EventLoop; }
namespace yy::core
{
    namespace zk { class ZkServiceClient; }
    namespace rpc { class RpcServer; }
    namespace redis { class RedisClient; }
    namespace mysql { class MySqlClient; }
    class IServer;
}

namespace yy::app::account
{
class AccountMysqlDAO;
class AccountRedisDAO;

class AccountRpcServiceImpl final : public protocol::app::AccountServiceRpc {
    constexpr static std::string PWD_field = "password";
    constexpr static std::string UID_field = "uid";

public:
    explicit AccountRpcServiceImpl(net::EventLoop * loop);

    void Login(google::protobuf::RpcController* controller,
               const protocol::app::LoginReq* request,
               protocol::app::LoginRsp* response,
               google::protobuf::Closure* done) override;

    void Register(google::protobuf::RpcController* controller,
                  const protocol::app::RegisterReq* request,
                  protocol::app::RegisterRsp* response,
                  google::protobuf::Closure* done) override;

private:
    auto GenerateToken() -> std::string;

    core::IServer & server_;
    core::rpc::RpcServer & rpc_server_;
    AccountRedisDAO& redis_dao_;
    AccountMysqlDAO& mysql_dao_;
    rpc_client::CenterRpcClient& center_client_;
};

}
