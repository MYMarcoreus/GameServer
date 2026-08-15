#pragma once

#include "room.pb.h"
#include "RpcClient.hpp"

namespace yy::app::rpc_client
{

    using GateRpcClient = core::rpc::RpcClient<protocol::app::GateRoomServiceRpc_Stub>;

}
