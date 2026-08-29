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

auto AccountMysqlDAO::GetAccountData(const std::string& username) -> std::expected<AccountData, std::error_code>
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
            return std::unexpected(GameError::kAccountNotFound);
        }
        // 验证密码
        const auto mysql_pwd = row[0].get<std::string>();
        const auto mysql_uid = row[1].get<uint64_t>();
        return AccountData{ username, mysql_pwd, mysql_uid };
    } catch (const mysqlx::Error& e) {
        YLOG_ERROR("AccountRpcServiceImpl::Login MySQL Query Error: {}", e.what())
        return std::unexpected(GameError::kDbError);
    }
}

auto AccountMysqlDAO::SetAccountData(const std::string& username, const std::string& password) -> std::expected<AccountData, std::error_code>
{
    const auto mysql_conn = mysql_client_.GetConnection();
    try {
        mysql_conn->conn.startTransaction();
        const auto reg_rst = mysql_conn->conn.getDefaultSchema()
            .getTable("account")
            .insert("username", "password")
            .values(username, password)
        .execute();
        YLOG_INFO("AccountRpcServiceImpl::Register Insert了 {} 条信息", reg_rst.getAffectedItemsCount())

        if (reg_rst.getAffectedItemsCount() == 0) {
            mysql_conn->conn.commit();
            return std::unexpected(GameError::kDuplicateUsername);
        }

        const auto uid = reg_rst.getAutoIncrementValue();
        mysql_conn->conn.commit();
        return AccountData{ username, password, uid };
    } catch (const mysqlx::Error& e) {
        YLOG_ERROR("AccountRpcServiceImpl::Register MySQL Query Error: {}", e.what())
        // 注册失败，回滚事务
        try { mysql_conn->conn.rollback(); } catch (...) { /* 忽略回滚失败 */ }
        // MySQL 唯一键冲突（ER_DUP_ENTRY）的错误消息稳定包含 "Duplicate entry"
        if (std::string(e.what()).find("Duplicate entry") != std::string::npos) {
            return std::unexpected(GameError::kDuplicateUsername);
        }
        return std::unexpected(GameError::kDbError);
    }
}
}
