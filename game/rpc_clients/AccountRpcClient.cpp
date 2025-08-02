#include "AccountRpcClient.h"

using namespace yy::protocol::app;
using yy::app::rpc_client::AccountRpcClient;

#define DEFINE_ACCOUNT_RPC_CALL(ReqType, RspType, MethodName)              \
template<>                                                                 \
template<>                                                                 \
void AccountRpcClient::DoCall<ReqType, RspType>(                           \
AccountServiceRpc_Stub& stub,                                              \
RpcControllerImpl* controller,                                             \
ReqType* request,                                                          \
RspType* response,                                                         \
google::protobuf::Closure* done)                                           \
{                                                                          \
stub.MethodName(controller, request, response, done);                      \
}

DEFINE_ACCOUNT_RPC_CALL(LoginReq, LoginRsp, Login)
DEFINE_ACCOUNT_RPC_CALL(RegisterReq, RegisterRsp, Register)
