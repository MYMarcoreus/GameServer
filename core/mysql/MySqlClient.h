#pragma once

#include "Singleton.h"
#include "log.h"
#include "MySqlPool.h"

namespace yy::core
{

class MySqlClient final : public Singleton<MySqlClient> {
    SINGLETON_NECESSITY(MySqlClient)
public:
    void Start(net::EventLoop * loop, const std::string& schema);

    auto GetConnection() const ->std::shared_ptr<MySqlConnection> { return pool_->Acquire(); }

    template<typename... Args>
    mysqlx::SqlResult Execute(const std::string& sql, Args&&... args)
    {
        try {
            const auto conn = pool_->Acquire();
            if (conn == nullptr) {
                return mysqlx::SqlResult();
            }
            mysqlx::SqlStatement stmt = conn->conn.sql(sql);
            // 如果bind是个不支持模板参数仅支持单个参数绑定的函数，则可以使用C++17的折叠表达式，一次性展开参数（而在C++17之前需要用模板递归）：bind(arg1), bind(arg2), ..., bind(arg100)
            (stmt.bind(std::forward<Args>(args)), ...);
            auto rst = stmt.execute();
            return rst;
        } catch (const mysqlx::Error& e) {
            YLOG_ERROR("MySQL Execute [{}] Error: {}", sql, e.what())
            throw;
        }
    }

    template<typename... Args>
    mysqlx::RowResult Query(const std::string& sql, Args&&... args)
    {
        try {
            const auto conn = pool_->Acquire();
            if (conn == nullptr) {
                return mysqlx::RowResult();
            }
            return conn->conn.sql(sql)
                             .bind(std::forward<Args>(args)...)
                             .execute();
        } catch (const mysqlx::Error& e) {
            YLOG_ERROR("MySQL Query [{}] Error: {}", sql, e.what())
            throw;
        }
    }

private:
    std::unique_ptr<MySqlPool> pool_ = nullptr;
    std::string schema_;
};

}
