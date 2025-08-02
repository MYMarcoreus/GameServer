#include "AccountMysqlDAO.h"
#include "MySqlClient.h"

namespace yy::app::account
{
AccountMysqlDAO::AccountMysqlDAO():
    mysql_client_(core::mysql::MySqlClient::Instance())
{
}

void AccountMysqlDAO::Start(net::EventLoop* loop)
{
    mysql_client_.Start(loop, "gameserver");
}

auto AccountMysqlDAO::GetAccountData(const std::string& username) -> std::optional<AccountData>
{
    try {
        // redis中没有该用户的信息，便去mysql去取
        const auto mysql_conn = mysql_client_.GetConnection();
        auto row_rst = mysql_conn->conn.getDefaultSchema()
            .getTable("account")
            .select(PWD_field, UID_field)
            .where("username = :usrname")
            .bind("usrname", username)
        .execute();

        auto row = row_rst.fetchOne();
        if (row.isNull()) {
            return std::nullopt;
        }
        // 验证密码
        const auto mysql_pwd = row[0].get<std::string>();
        const auto mysql_uid = row[1].get<uint64_t>();
        AccountData data( username, std::move(mysql_pwd), mysql_uid);
        return data;
    } catch (const mysqlx::Error& e) {
        YLOG_ERROR("AccountRpcServiceImpl::Login MySQL Query Error: {}", e.what())
        return std::nullopt;
    }
}

auto AccountMysqlDAO::HasAccountData(const std::string& username) -> bool
{
    try {
        const auto mysql_conn = mysql_client_.GetConnection();
        // 查询账号是否存在
        auto row_rst = mysql_conn->conn.getDefaultSchema()
            .getTable("account")
            .select("1")
            .where("username = :usrname")
            .limit(1)
            .bind("usrname", username)
        .execute();
        const bool is_exist = row_rst.count() ;
        return is_exist;
    }  catch (const mysqlx::Error& e) {
        YLOG_ERROR("AccountRpcServiceImpl::Register MySQL Query Error: {}", e.what())
        return false;
    }
}

auto AccountMysqlDAO::SetAccountData(const std::string& username, const std::string& password) -> std::optional<AccountData>
{
    const auto mysql_conn = mysql_client_.GetConnection();
    mysql_conn->conn.startTransaction();
    try {
        // 账号不存在则注册
        std::optional<AccountData> data = std::nullopt;
        const auto reg_rst = mysql_conn->conn.getDefaultSchema()
            .getTable("account")
            .insert("username", "password")
            .values(username, password)
        .execute();
        YLOG_INFO("AccountRpcServiceImpl::Register Insert了 {} 条信息", reg_rst.getAffectedItemsCount())
        if (reg_rst.getAffectedItemsCount() > 0) {
            const auto uid = reg_rst.getAutoIncrementValue();
            data = AccountData(username, password, uid);
        }

        // 注册成功，提交事务
        mysql_conn->conn.commit();
        return data;
    } catch (const mysqlx::Error& e) {
        YLOG_ERROR("AccountRpcServiceImpl::Register MySQL Query Error: {}", e.what())
        // 注册失败，回滚事务
        mysql_conn->conn.rollback();
        return std::nullopt;
    }
}
}
