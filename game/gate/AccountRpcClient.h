#pragma once

#include "account.pb.h"
#include "RpcClient.hpp"

namespace yy::app
{

using AccountRpcClient = core::rpc::RpcClient<protocol::app::AccountServiceRpc_Stub>;

}
