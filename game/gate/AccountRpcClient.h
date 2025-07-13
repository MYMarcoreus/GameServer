#pragma once

#include "account.pb.h"
#include "RpcClient.hpp"

namespace yy::app::gate
{

using AccountRpcClient = core::RpcClient<protocol::app::AccountServiceRpc_Stub>;

}
