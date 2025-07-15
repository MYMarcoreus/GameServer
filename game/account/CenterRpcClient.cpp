#include "CenterRpcClient.h"

using namespace yy::protocol::app;
using yy::app::CenterRpcClient;

template<>
template<>
void CenterRpcClient::DoCall<SelectServerReq, SelectServerRsp>(
    CenterServiceRpc_Stub& stub,
    yy::core::rpc::RpcControllerImpl* controller,
    SelectServerReq* request,
    SelectServerRsp* response,
    google::protobuf::Closure* done)
{
    stub.SelectServer(controller, request, response, done);
}

