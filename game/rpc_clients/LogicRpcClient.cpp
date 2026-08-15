#include "LogicRpcClient.h"

using namespace yy::protocol::app;
using yy::app::rpc_client::LogicRpcClient;

#define DEFINE_CENTER_RPC_CALL(ReqType, RspType, MethodName)           \
template<>                                                             \
template<>                                                             \
void LogicRpcClient::DoCall<ReqType, RspType>(                         \
LogicRoomServiceRpc_Stub& stub,                                        \
RpcControllerImpl* controller,                                         \
const ReqType* request,                                                \
RspType* response,                                                     \
google::protobuf::Closure* done)                                       \
{                                                                      \
stub.MethodName(controller, request, response, done);                  \
}

DEFINE_CENTER_RPC_CALL(NewRoomReq, NewRoomRsp, NewRoom)
DEFINE_CENTER_RPC_CALL(DeleteRoomReq, DeleteRoomRsp, DeleteRoom)
DEFINE_CENTER_RPC_CALL(GetLogicAddrReq, GetLogicAddrRsp, GetLogicAddr)
