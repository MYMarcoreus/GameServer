#include "AccountRpcServiceImpl.h"

#include "AccountMysqlDAO.h"
#include "IPAddress.h"
#include "log.h"
#include "AccountServerManager.h"
#include "RedisClient.h"
#include "ZkServiceClient.h"
#include "MySqlClient.h"
#include "RemoteXmlConfig.h"
#include "RpcServer.h"
#include "AccountRedisDAO.h"


namespace yy::app::account
{
AccountRpcServiceImpl::AccountRpcServiceImpl(EventLoop * loop):
    server_(AccountServerManager::Instance().GetServer()),
    rpc_server_(AccountServerManager::Instance().GetRpcServer()),
    redis_dao_(AccountRedisDAO::Instance()),
    mysql_dao_(AccountMysqlDAO::Instance()),
    center_client_(rpc_client::CenterRpcClient::Instance())
{
    // 初始化Redis
    redis_dao_.Start(loop);
    // 初始化MySql
    mysql_dao_.Start(loop);

    center_client_.Start(
        [](const TcpConnectionPtr & ) {
        YLOG_INFO("连接至CenterRpc服务器！");
    });
}

// 该函数仅需填充response并调用done->Run()
void AccountRpcServiceImpl::Login(google::protobuf::RpcController* controller,
    const LoginReq* request, LoginRsp* response, google::protobuf::Closure* done)
{
    YLOG_INFO("正在执行 AccountRpcServiceImpl::Login 服务，填充响应体");

    const std::string& username = request->username();
    const std::string& password = request->password();

    // 在Mysql获取并验证账号数据
    const auto account_data  = mysql_dao_.GetAccountData(username);
    LoginRsp_Status login_status = LoginRsp_Status_eSuccess;
    if (!account_data.has_value()) {
        login_status = LoginRsp_Status_eAccountNotExist;
    } else if (account_data->password != password) {
        login_status = LoginRsp_Status_ePasswordError;
    }

    // 构造响应
    response->set_result_code(login_status);
    if (login_status == LoginRsp_Status_eSuccess) {
        // 生成token并保存至redis
        auto token = GenerateToken();
        redis_dao_.SetToken(account_data->uid, token);
        response->set_token(token);

        response->mutable_account_data()->set_uid(account_data->uid);
        response->mutable_account_data()->set_username(username);
    }

    // 发送响应
    done->Run();
}

void AccountRpcServiceImpl::Register(google::protobuf::RpcController* controller,
    const RegisterReq* request, RegisterRsp* response, google::protobuf::Closure* done)
{
    YLOG_TRACE("正在执行 AccountRpcServiceImpl::Register 服务，填充响应体")

    // 验证注册
    if (not mysql_dao_.HasAccountData(request->username())) {
        const auto rst = mysql_dao_.SetAccountData(request->username(), request->password());
        response->set_uid(rst->uid);
        response->set_result_code(RegisterRsp_Status_eSuccess);
    } else {
        response->set_result_code(RegisterRsp_Status_eAccountAlreadyExist);
    }

    // 发送响应
    done->Run();
}

auto AccountRpcServiceImpl::GenerateToken() -> std::string
{
    return util::GenerateToken();
}
}
