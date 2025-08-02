#pragma once

#include "room.pb.h"
#include "RpcClient.hpp"

namespace yy::app::rpc_client
{

using CenterRpcClient = core::rpc::RpcClient<protocol::app::CenterRoomServiceRpc_Stub>;

}
