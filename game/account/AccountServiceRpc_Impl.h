#pragma once

#include "account.pb.h"
#include "CenterRpcClient.h"

namespace yy::net { class EventLoop; }
namespace yy::core
{
    namespace rpc { class RpcServer; }
    namespace redis { class RedisClient; }
    namespace mysql { class MySqlClient; }
    class IServer;
}

namespace yy::app::account
{
class AccountMysqlDAO;
class AccountRedisDAO;

class AccountServiceRpc_Impl final : public protocol::app::AccountServiceRpc {
    constexpr static std::string PWD_field = "password";
    constexpr static std::string UID_field = "uid";

public:
    explicit AccountServiceRpc_Impl(net::EventLoop * loop);

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

    AccountRedisDAO& redis_dao_;
    AccountMysqlDAO& mysql_dao_;
    rpc_client::CenterRpcClient& center_client_;
};

}
