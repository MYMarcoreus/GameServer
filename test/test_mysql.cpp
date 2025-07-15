#include "log.h"
#include "MySqlPool.h"
#include "ConfigManager.h"
#include "EventLoop.h"
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

    loop->RunEvery(1s, [&loop]()
    {
        auto & mysql_client = yy::core::MySqlClient::Instance();
        mysql_client.Start(loop, "gameserver");
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


