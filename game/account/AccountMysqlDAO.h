#pragma once
#include <optional>
#include <string>

#include "AccountData.h"
#include "Singleton.h"

namespace yy::core::mysql
{
class MySqlClient;
}

namespace yy::net
{
class EventLoop;
}

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

    auto GetAccountData(const std::string& username) -> std::optional<AccountData>;
    auto HasAccountData(const std::string& username) -> bool;
    auto SetAccountData(const std::string& username, const std::string& password) -> std::optional<AccountData>;


private:
    core::mysql::MySqlClient& mysql_client_;
};



}
