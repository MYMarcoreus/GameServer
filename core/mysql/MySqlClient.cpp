#include "MySqlClient.h"

namespace yy::core
{
void MySqlClient::Start(net::EventLoop* loop, const std::string& ip, int port, const std::string& user,
    const std::string& pwd, const std::string& schema, size_t pool_size)
{
    if (pool_ == nullptr) {
        pool_ = std::make_unique<MySqlPool>(loop, ip, port, user, pwd, schema, pool_size);
    }
}
}
