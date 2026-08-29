#pragma once
#include <expected>
#include <string>
#include <system_error>

#include "AccountData.h"
#include "Singleton.h"
#include "../errors.h"

namespace yy::core::mysql { class MySqlClient; }
namespace yy::net { class EventLoop; }

namespace yy::app::account
{

class AccountMysqlDAO final : public Singleton<AccountMysqlDAO>
{
    SINGLETON_NECESSITY(AccountMysqlDAO);
    constexpr static std::string PWD_field = "password";
    constexpr static std::string UID_field = "uid";
public:
    explicit AccountMysqlDAO();
    void Start(net::EventLoop* loop);

    //! 查询账号：错误码区分“账号不存在”(kAccountNotFound) 与“数据库错误”(kDbError)
    auto GetAccountData(const std::string& username) -> std::expected<AccountData, std::error_code>;

    //! 注册账号：原子完成（内部通过唯一键冲突识别重复用户名，调用方无需先查重，避免 TOCTOU）
    //! 错误码：kDuplicateUsername（用户名已存在）/ kDbError（数据库错误）
    auto SetAccountData(const std::string& username, const std::string& password) -> std::expected<AccountData, std::error_code>;


private:
    core::mysql::MySqlClient& mysql_client_;
};



}
