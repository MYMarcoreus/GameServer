#include "GateRpcServiceImpl.h"

#include "ForwardManager.h"
#include "GateServerManager.h"

using namespace yy::protocol::app;
using namespace yy::net;

namespace yy::app::gate
{
GateRpcServiceImpl::GateRpcServiceImpl(EventLoop* base_loop, ForwardManager& frontend):
    forwarder_(frontend)
{
}

void GateRpcServiceImpl::BroadcastRoom(google::protobuf::RpcController* controller, const BroadcastRoomReq* request,
    BroadcastRoomRsp* response, google::protobuf::Closure* done)
{
    YLOG_INFO("正在执行 GateRpcServiceImpl::BroadcastRoom 服务：{}", request->ShortDebugString())

    // 广播给前端
    forwarder_.BroadcastToFrontend(*request);

    // 广播成功
    done->Run();
}
}
