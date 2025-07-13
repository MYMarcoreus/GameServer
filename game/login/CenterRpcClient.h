#pragma once

#include "center.pb.h"
#include "RpcClient.hpp"

namespace yy::app::center
{

using CenterRpcClient = core::RpcClient<protocol::app::CenterServiceRpc_Stub>;

}
