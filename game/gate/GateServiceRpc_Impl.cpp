#include "GateServiceRpc_Impl.h"

#include "ForwardManager.h"
#include "GateServerManager.h"

using namespace yy::protocol::app;
using namespace yy::net;

namespace yy::app::gate
{
GateServiceRpc_Impl::GateServiceRpc_Impl(EventLoop* base_loop, ForwardManager& frontend):
    forwarder_(frontend)
{
}

void GateServiceRpc_Impl::BroadcastRoom(google::protobuf::RpcController* controller, const BroadcastRoomReq* request,
    BroadcastRoomRsp* response, google::protobuf::Closure* done)
{
    YLOG_INFO("正在执行 GateServiceRpc_Impl::BroadcastRoom 服务：{}", request->ShortDebugString())

    // 广播给前端
    forwarder_.BroadcastToFrontend(*request);

    // 广播成功
    done->Run();
}
}
