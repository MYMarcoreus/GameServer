#include "AccountRpcService.h"

#include "log.h"

namespace yy::app::login
{
void AccountRpcService::Login(google::protobuf::RpcController* controller, const yy::protocol::app::C2SLogin* request,
    yy::protocol::app::S2CLogin* response, google::protobuf::Closure* done)
{
    YLOG_TRACE("正在执行 AccountRpcService::Login 服务，填充响应体")

    response->set_account_id(2333);
    response->set_account_name("testid");
    response->set_logic_server_id(114514);
    response->set_result_code(protocol::app::S2CLogin_Status_eSuccess);
    response->set_session_id(20250625);

    done->Run();
}

void AccountRpcService::Register(google::protobuf::RpcController* controller, const yy::protocol::app::C2SRegister* request,
    yy::protocol::app::S2CRegister* response, google::protobuf::Closure* done)
{
    YLOG_TRACE("正在执行 AccountRpcService::Register 服务，填充响应体")
    response->set_session_id(114514);
    response->set_result_code(protocol::app::S2CRegister_Status_eSuccess);

    done->Run();
}
}
