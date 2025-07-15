#include "MySqlClient.h"
#include "RemoteXmlConfig.h"

namespace yy::core
{
void MySqlClient::Start(net::EventLoop* loop, const std::string& schema)
{
    if (pool_ == nullptr) {
        decltype(yy::config::g_remote_config->GetValue().m_remote_nodes)::value_type mysql_configs;
        for (auto & node: yy::config::g_remote_config->GetValue().m_remote_nodes) {
            if (node.type == "mysql") {
                mysql_configs = node;
            }
        }
        schema_ = schema;
        pool_ = std::make_unique<MySqlPool>(loop, mysql_configs.ip, mysql_configs.port, mysql_configs.username, mysql_configs.password, schema, mysql_configs.poolsize);
    }
}
}
