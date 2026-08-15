#include "AccountServiceRpc_Impl.h"

#include "AccountMysqlDAO.h"
#include "IPAddress.h"
#include "log.h"
#include "AccountServerManager.h"
#include "RedisClient.h"
#include "MySqlClient.h"
#include "AccountRedisDAO.h"

using namespace yy::net;
using namespace yy::core;
using namespace yy::protocol::app;

namespace yy::app::account
{
AccountServiceRpc_Impl::AccountServiceRpc_Impl(EventLoop * loop):
    redis_dao_(AccountRedisDAO::Instance()),
    mysql_dao_(AccountMysqlDAO::Instance()),
    center_client_(rpc_client::CenterRpcClient::Instance())
{
    // 初始化Redis
    redis_dao_.Start(loop);
    // 初始化MySql
    mysql_dao_.Start(loop);

    center_client_.SetConnectionEstablishedCallback([](const TcpConnectionPtr & ) {
            YLOG_INFO("连接至CenterRpc服务器！");
        });
}

// 该函数仅需填充response并调用done->Run()
void AccountServiceRpc_Impl::Login(google::protobuf::RpcController* controller,
    const LoginReq* request, LoginRsp* response, google::protobuf::Closure* done)
{
    YLOG_INFO("正在执行 AccountServiceRpc_Impl::Login 服务，填充响应体");

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
        const UID_t uid = account_data->uid;

        // 检查重复登录
        const auto existing_token = redis_dao_.GetToken(uid);
        if (existing_token.has_value()) {
            // 此处根据业务选择踢出旧登录 或 拒绝新登录
            login_status = LoginRsp_Status_eAlreadyLoggedIn;
            response->set_result_code(login_status);
            done->Run();
            return;
        }

        // 生成token并保存至redis
        auto token = GenerateToken();
        redis_dao_.SetToken(uid, token);
        response->set_token(token);

        response->mutable_account_data()->set_uid(uid);
        response->mutable_account_data()->set_username(username);
    }

    // 发送响应
    done->Run();
}

void AccountServiceRpc_Impl::Register(google::protobuf::RpcController* controller,
    const RegisterReq* request, RegisterRsp* response, google::protobuf::Closure* done)
{
    YLOG_TRACE("正在执行 AccountServiceRpc_Impl::Register 服务，填充响应体")

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

auto AccountServiceRpc_Impl::GenerateToken() -> std::string
{
    return util::GenerateToken();
}
}
