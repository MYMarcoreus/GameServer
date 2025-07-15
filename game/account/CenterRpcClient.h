#pragma once

#include "center.pb.h"
#include "RpcClient.hpp"

namespace yy::app
{

using CenterRpcClient = core::rpc::RpcClient<protocol::app::CenterServiceRpc_Stub>;

}
