#include "AccountRpcClient.h"

using namespace yy::protocol::app;
using yy::app::AccountRpcClient;

template<>
template<>
void AccountRpcClient::DoCall<C2SLogin, S2CLogin>(
    AccountServiceRpc_Stub& stub,
    yy::core::rpc::RpcControllerImpl* controller,
    C2SLogin* request,
    S2CLogin* response,
    google::protobuf::Closure* done)
{
    stub.Login(controller, request, response, done);
}

template<>
template<>
void AccountRpcClient::DoCall<C2SRegister, S2CRegister>(
    AccountServiceRpc_Stub& stub,
    yy::core::rpc::RpcControllerImpl* controller,
    C2SRegister* request,
    S2CRegister* response,
    google::protobuf::Closure* done)
{
    stub.Register(controller, request, response, done);
}
