#include "log.h"
#include "MySqlPool.h"
#include "ConfigManager.h"
#include "RemoteXmlConfig.h"

using namespace std::chrono_literals;

int main(int argc, char* argv[])
{
    yy::config::ConfigManager::AddFilePath("../config/configs_logic.xml");
    yy::config::ConfigManager::AddFilePath("../../config/configs_logic.xml");
    yy::config::ConfigManager::LoadXmlConfigs();
    yy::Ylog::LoggerManager::getInstance().ReadConfigs();

    auto mysql_configs = yy::config::g_remote_config->GetValue().m_mysql_configs;
    auto loop = new yy::net::EventLoop(500ms);

    loop->RunEvery(1s, [&loop, &mysql_configs]()
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
        YLOG_INFO("db name = {}", name);
    });

    loop->Loop();
}


