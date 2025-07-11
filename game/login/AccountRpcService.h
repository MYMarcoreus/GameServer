#pragma once

#include "login.pb.h"
#include <google/protobuf/service.h>

namespace yy::app::login
{

class AccountRpcService final : public yy::protocol::app::AccountServiceRpc {
    void Login(google::protobuf::RpcController* controller,
               const ::yy::protocol::app::C2SLogin* request,
               yy::protocol::app::S2CLogin* response,
               google::protobuf::Closure* done) override;

    void Register(google::protobuf::RpcController* controller,
                  const yy::protocol::app::C2SRegister* request,
                  yy::protocol::app::S2CRegister* response,
                  google::protobuf::Closure* done) override;
};

}
