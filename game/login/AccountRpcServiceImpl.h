#pragma once

#include "account.pb.h"

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

namespace yy::app::login
{

class AccountRpcServiceImpl final : public yy::protocol::app::AccountServiceRpc {
public:
    AccountRpcServiceImpl();

    void Login(google::protobuf::RpcController* controller,
               const ::yy::protocol::app::C2SLogin* request,
               yy::protocol::app::S2CLogin* response,
               google::protobuf::Closure* done) override;

    void Register(google::protobuf::RpcController* controller,
                  const yy::protocol::app::C2SRegister* request,
                  yy::protocol::app::S2CRegister* response,
                  google::protobuf::Closure* done) override;

private:
    auto GetToken() -> std::string;
    auto GetLogicServerAddr() -> std::pair<std::string, uint16_t>;

    yy::core::RedisClient& redis_client_;
    yy::core::MySqlClient& mysql_pool_;
    yy::core::zk::ZkServiceManager & zk_;

};

}
