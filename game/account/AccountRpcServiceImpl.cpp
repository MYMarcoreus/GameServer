#include "AccountRpcServiceImpl.h"
#include "IPAddress.h"
#include "log.h"
#include "AccountServerManager.h"
#include "RedisClient.h"
#include "ZkServiceManager.h"
#include "MySqlClient.h"
#include "RemoteXmlConfig.h"
#include "RpcServer.h"

using yy::protocol::app::SelectServerReq;
using yy::protocol::app::SelectServerRsp;

namespace yy::app::account
{
AccountRpcServiceImpl::AccountRpcServiceImpl(net::EventLoop * loop):
    server_(AccountServerManager::Instance().GetServer()),
    rpc_server_(AccountServerManager::Instance().GetRpcServer()),
    redis_client_(core::RedisClient::Instance()),
    mysql_client_(core::MySqlClient::Instance()),
    center_client_(CenterRpcClient::Instance())
{
    // 初始化Redis
    redis_client_.Start(loop, 5);
    // 初始化MySql
    mysql_client_.Start(loop, "gameserver");
    // 初始化ZkClient
    center_client_.Start(5, [](const net::TcpConnectionPtr & conn) {
        YLOG_INFO("连接至CenterRpc服务器！");
    });
}

// 该函数仅需填充response并调用done->Run()
void AccountRpcServiceImpl::Login(google::protobuf::RpcController* controller, const yy::protocol::app::C2SLogin* request,
                              yy::protocol::app::S2CLogin* response, google::protobuf::Closure* done)
{
    YLOG_INFO("正在执行 AccountRpcServiceImpl::Login 服务，填充响应体")

    SelectServerReq select_server_req;
    select_server_req.set_session_id(request->session_id());
    select_server_req.set_username(request->username());

     yy::protocol::app::S2CLogin_Status login_status = protocol::app::S2CLogin_Status_eSuccess;

    // 验证账号密码
    const auto redi_pwd = redis_client_.Get(request->username());
    if (redi_pwd.has_value()) {
        // 验证密码
        if (redi_pwd.value() == request->password()) {
            login_status = protocol::app::S2CLogin_Status_eSuccess;
        } else {
            login_status = protocol::app::S2CLogin_Status_ePasswordError;
        }
    } else {
        try {
            // redis中没有该用户的信息，便去mysql去取
            // auto rst = mysql_client_.Query("SELECT password FROM account WHERE username == ?", request->username());
            const auto mysql_conn = mysql_client_.GetConnection();
            auto row_rst = mysql_conn->conn.getDefaultSchema()
                .getTable("account")
                .select("password")
                .where("username = :usrname")
                .bind("usrname", request->username())
            .execute();
            auto row = row_rst.fetchOne();

            if (row.isNull()) {
                // mysql账号不存在
                login_status = protocol::app::S2CLogin_Status_eAccountNotExist;
            } else {
                // 验证密码
                const auto mysql_pwd = row[0].get<std::string>();
                if (mysql_pwd == request->password()) {
                    login_status = protocol::app::S2CLogin_Status_eSuccess;
                    redis_client_.Set(request->username(), request->password());
                } else {
                    login_status = protocol::app::S2CLogin_Status_ePasswordError;
                }
            }
        }  catch (const mysqlx::Error& e) {
            YLOG_ERROR("AccountRpcServiceImpl::Login MySQL Query Error: {}", e.what())
            login_status = protocol::app::S2CLogin_Status_eUnknownError;
        }
    }

    // 验证不成功，发送消息并返回
    if (login_status != protocol::app::S2CLogin_Status_eSuccess) {
        response->set_session_id(request->session_id());
        response->set_username(request->username());
        response->set_result_code(login_status);
        done->Run();
        return;
    }

    // 账号密码验证成功，发起异步远程调用，获取后端分配给客户端的服务器
    center_client_.CallRemoteAsync<SelectServerReq, SelectServerRsp>(select_server_req,
        // 异步函数，需要复制数据
        [this, response, done](std::unique_ptr<SelectServerRsp> && resp, std::unique_ptr<core::rpc::RpcControllerImpl> && controller) {
            switch (resp->result_code()) {
            case protocol::app::SelectServerRsp_Status_eSuccess:
                response->set_session_id(resp->session_id());
                response->set_result_code(protocol::app::S2CLogin_Status_eSuccess);

                response->set_username(resp->username());
                response->set_ip(resp->ip());
                response->set_port(resp->port());
                response->set_token(resp->token());
                break;
            case protocol::app::SelectServerRsp_Status_eNoServer:
                response->set_session_id(resp->session_id());
                response->set_result_code(protocol::app::S2CLogin_Status_eUnknownError);
                break;
            default: ;
            }

            // 发送S2CLogin * response消息并析构之
            done->Run();
        });
}

void AccountRpcServiceImpl::Register(google::protobuf::RpcController* controller, const yy::protocol::app::C2SRegister* request,
    yy::protocol::app::S2CRegister* response, google::protobuf::Closure* done)
{
    YLOG_TRACE("正在执行 AccountRpcServiceImpl::Register 服务，填充响应体")

    yy::protocol::app::S2CRegister_Status reg_status = protocol::app::S2CRegister_Status_eSuccess;

    const auto mysql_conn = mysql_client_.GetConnection();

    mysql_conn->conn.startTransaction();
    try {
        // 查询账号是否存在
        auto row_rst = mysql_conn->conn.getDefaultSchema()
            .getTable("account")
            .select("1")
            .where("username = :usrname")
            .bind("usrname", request->username())
        .execute();

        const bool is_exist = row_rst.count() ;
        if (is_exist) {
            reg_status = protocol::app::S2CRegister_Status_eAccountAlreadyExist;
        } else {
            // 账号不存在则注册
            const auto reg_rst = mysql_conn->conn.getDefaultSchema()
                .getTable("account")
                .insert("username", "password")
                .values(request->username(), request->password())
            .execute();
            YLOG_INFO("AccountRpcServiceImpl::Register Insert了 {} 条信息", reg_rst.getAffectedItemsCount())
            if (reg_rst.getAffectedItemsCount() > 0) {
                reg_status = protocol::app::S2CRegister_Status_eSuccess;
            } else {
                reg_status = protocol::app::S2CRegister_Status_eUnknownError;
            }
        }
        mysql_conn->conn.commit();
    } catch (const mysqlx::Error& e) {
        YLOG_ERROR("AccountRpcServiceImpl::Register MySQL Query Error: {}", e.what())
        reg_status = protocol::app::S2CRegister_Status_eUnknownError;
        mysql_conn->conn.rollback();
    }

    response->set_session_id(request->session_id());
    response->set_result_code(reg_status);
    done->Run();
}

}
