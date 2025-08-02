#pragma once

#include "account.pb.h"
#include "RpcClient.hpp"

namespace yy::app::rpc_client
{

using AccountRpcClient = core::rpc::RpcClient<protocol::app::AccountServiceRpc_Stub>;

}
