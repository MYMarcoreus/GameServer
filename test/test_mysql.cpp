#include "log.h"
#include "MySqlPool.h"
#include "ConfigManager.h"
#include "MySqlClient.h"
#include "RemoteXmlConfig.h"

using namespace std::chrono_literals;

int main(int argc, char* argv[])
{
    yy::config::ConfigManager::AddFilePath("../config/configs_logic.xml");
    yy::config::ConfigManager::AddFilePath("../../config/configs_logic.xml");
    yy::config::ConfigManager::LoadXmlConfigs();
    yy::Ylog::LoggerManager::Instance().ReadConfigs();

    decltype(yy::config::g_remote_config->GetValue().m_remote_nodes)::value_type mysql_configs;
    for (auto & node: yy::config::g_remote_config->GetValue().m_remote_nodes) {
        if (node.type == "mysql") {
            mysql_configs = node;
        }
    }
    auto loop = new yy::net::EventLoop(500ms);

    loop->RunEvery(5s, [&loop, &mysql_configs]()
    {
        std::shared_ptr<yy::core::MySqlPool> pool = std::make_shared<yy::core::MySqlPool>(
            loop,
            mysql_configs.ip,           // IP 地址
            mysql_configs.port,         // MySQL X Protocol 端口（注意不是3306）
            mysql_configs.username,     // 用户名
            mysql_configs.password,     // 密码
            "gameserver",               // Schema / 数据库名
            mysql_configs.poolsize      // 池大小
        );
        auto mysql_conn = pool->Acquire();
        std::string name = mysql_conn->conn.getDefaultSchema().getName();
        YLOG_INFO("db name = {}, ", name);
    });

    loop->RunEvery(1s, [&loop, &mysql_configs]()
    {
        auto & mysql_client = yy::core::MySqlClient::Instance();
        mysql_client.Start(
            loop,
            mysql_configs.ip,           // IP 地址
            mysql_configs.port,         // MySQL X Protocol 端口（注意不是3306）
            mysql_configs.username,     // 用户名
            mysql_configs.password,     // 密码
            "gameserver",               // Schema / 数据库名
            mysql_configs.poolsize      // 池大小
        );
        auto result1 = mysql_client.Query("SELECT * FROM account WHERE uid = ?;", "1");
        YLOG_INFO("SELECT = {}", result1.count());
        auto result2 = mysql_client.Execute("SELECT * FROM account WHERE uid = ?;", "1");
        if (result2.hasData()) {
            YLOG_INFO("SELECT = {}", result2.count());
        }
        else {
            YLOG_INFO("result2 not data", result2.count());
        }
    });

    auto token = yy::util::GenerateToken(32);
    std::cout << token.size() << std::endl;
    std::cout << token << std::endl;


    loop->Loop();
}


