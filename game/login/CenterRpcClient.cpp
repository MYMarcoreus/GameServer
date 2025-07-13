#include "CenterRpcClient.h"

using namespace yy::protocol::app;
using yy::app::center::CenterRpcClient;

template<>
template<>
void CenterRpcClient::DoCall<C2SSelectServer, S2CSelectServer>(
    CenterServiceRpc_Stub& stub,
    yy::core::RpcControllerImpl* controller,
    C2SSelectServer* request,
    S2CSelectServer* response,
    google::protobuf::Closure* done)
{
    stub.SelectServer(controller, request, response, done);
}

