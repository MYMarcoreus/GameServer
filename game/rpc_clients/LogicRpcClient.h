#pragma once

#include "inner_room.pb.h"
#include "RpcClient.hpp"

namespace yy::app::rpc_client
{

using LogicRpcClient = core::rpc::RpcClient<protocol::app::LogicRoomServiceRpc_Stub>;

}
