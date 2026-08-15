#include "GateRpcClient.h"

using namespace yy::protocol::app;
using yy::app::rpc_client::GateRpcClient;

#define DEFINE_CENTER_RPC_CALL(ReqType, RspType, MethodName)           \
template<>                                                             \
template<>                                                             \
void GateRpcClient::DoCall<ReqType, RspType>(                          \
GateRoomServiceRpc_Stub& stub,                                         \
RpcControllerImpl* controller,                                         \
const ReqType* request,                                                \
RspType* response,                                                     \
google::protobuf::Closure* done)                                       \
{                                                                      \
stub.MethodName(controller, request, response, done);                  \
}

DEFINE_CENTER_RPC_CALL(BroadcastRoomReq, BroadcastRoomRsp, BroadcastRoom)
