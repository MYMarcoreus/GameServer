#pragma once


#include "room.pb.h"

namespace yy::net
{
class EventLoop;
}

namespace yy::app::gate
{
class ForwardManager;

class GateServiceRpc_Impl final : public protocol::app::GateRoomServiceRpc {
public:
    explicit GateServiceRpc_Impl(net::EventLoop * base_loop, ForwardManager& frontend);

    void BroadcastRoom(google::protobuf::RpcController* controller, const protocol::app::BroadcastRoomReq* request,
                       protocol::app::BroadcastRoomRsp* response, google::protobuf::Closure* done) override;

private:
    ForwardManager& forwarder_;
};

}
