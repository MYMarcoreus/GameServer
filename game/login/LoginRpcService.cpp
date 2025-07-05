#include "LoginRpcService.h"

#include "log.h"

namespace yy::app::login
{
void LoginRpcService::Login(google::protobuf::RpcController* controller, const yy::protocol::app::C2SLogin* request,
    yy::protocol::app::S2CLogin* response, google::protobuf::Closure* done)
{
    // AccountServiceRpc::Login(controller, request, response, done);
    YLOG_TRACE("正在执行 LoginRpcService::Login 服务，填充响应体")

    response->set_account_id(2333);
    response->set_account_name("testid");
    response->set_logic_server_id(114514);
    response->set_result_code(protocol::app::S2CLogin_Status_eSuccess);
    response->set_session_id(20250625);

    done->Run();
}

}
