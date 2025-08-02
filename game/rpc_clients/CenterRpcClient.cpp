#include "CenterRpcClient.h"

using namespace yy::protocol::app;
using yy::app::rpc_client::CenterRpcClient;

#define DEFINE_CENTER_RPC_CALL(ReqType, RspType, MethodName)               \
template<>                                                                 \
template<>                                                                 \
void CenterRpcClient::DoCall<ReqType, RspType>(                            \
CenterRoomServiceRpc_Stub& stub,                                             \
RpcControllerImpl* controller,                                         \
ReqType* request,                                                      \
RspType* response,                                                     \
google::protobuf::Closure* done)                                       \
{                                                                          \
stub.MethodName(controller, request, response, done);                 \
}

DEFINE_CENTER_RPC_CALL(JoinRoomReq, JoinRoomRsp, JoinRoom)
DEFINE_CENTER_RPC_CALL(CreateRoomReq, CreateRoomRsp, CreateRoom)
DEFINE_CENTER_RPC_CALL(GetEnterSceneTokenReq, GetEnterSceneTokenRsp, GetEnterSceneToken)
DEFINE_CENTER_RPC_CALL(SearchRoomReq, SearchRoomRsp, SearchRoom)
DEFINE_CENTER_RPC_CALL(QuitRoomReq, QuitRoomRsp, QuitRoom)
