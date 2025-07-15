#include "MySqlPool.h"
#include "log.h"
#include "EventLoop.h"
#include <iostream>

using namespace std::chrono_literals;

namespace yy::core
{
MySqlPool::MySqlPool(yy::net::EventLoop * loop, const std::string& ip, const int port, const std::string& user, const std::string& pwd, const std::string& schema, const size_t poolSize)
    : loop_(loop), ip_(ip), port_(port), user_(user), pass_(pwd), schema_(schema), max_size_(poolSize), _fail_count(0)
{
    try {
        for (int i = 0; i < max_size_; ++i) {
            const mysqlx::SessionSettings settings(ip_, port_, user_, pass_, schema_); // 33060 是默认 X Protocol 端口
            mysqlx::Session conn(settings);
            conn.getSchema(schema_); // 确保 schema 存在

            // 获取当前时间
            auto currentTime = std::chrono::system_clock::now().time_since_epoch();
            long long timestamp = std::chrono::duration_cast<std::chrono::seconds>(currentTime).count();
            pool_.push(std::make_unique<MySqlConnection>(std::move(conn), timestamp));
        }

        checker_timer_id_ = loop_->RunEvery(60s, [this]()
        {
            if (not is_stop_) {
                YLOG_INFO("MySqlPool check_connection!");
                check_connection();
            }
        });
    }
    catch (const mysqlx::Error& e) {
        std::cerr << "MySqlPool init failed, error is " << e.what()<< std::endl;
        std::terminate();
    }
}

MySqlPool::~MySqlPool()
{
    std::unique_lock lg(mutex_);
    loop_->CancelTimer(checker_timer_id_);
    is_stop_ = true;
    while (!pool_.empty()) {
        pool_.pop();
    }
}

void MySqlPool::check_connection()
{
    // 1)先读取“目标处理数”
    size_t targetCount = 0;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        targetCount = pool_.size();
    }

    //2 当前已经处理的数量
    size_t processed = 0;

    //3 时间戳
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const long long timestamp = std::chrono::duration_cast<std::chrono::seconds>(now).count();

    while (processed < targetCount) {
        std::unique_ptr<MySqlConnection> con;
        {
            std::lock_guard lg(mutex_);
            if (pool_.empty()) {
                break;
            }
            con = std::move(pool_.front());
            pool_.pop();
        }

        bool healthy = true;
        //解锁后做检查/重连逻辑
        if (timestamp - con->last_oper_time >= 5) {
            try {
                con->conn.sql("SELECT 1").execute().count() > 0;
                con->last_oper_time = timestamp;
            }
            catch (const mysqlx::Error& e) {
                std::cerr << "Error keeping mysql connection alive: " << e.what() << std::endl;
                healthy = false;
                ++_fail_count;
            }

        }

        if (healthy)
        {
            std::lock_guard lg(mutex_);
            pool_.push(std::move(con));
            cond_.notify_one();
        }

        ++processed;
    }


    while (_fail_count > 0) {
        if (reconnect(timestamp)) {
            --_fail_count;
        }
        else {
            break;
        }
    }
}

bool MySqlPool::reconnect(long long timestamp)
{
    try {
        const mysqlx::SessionSettings settings(ip_, port_, user_, pass_, schema_); // 33060 是默认 X Protocol 端口
        mysqlx::Session conn(settings);
        auto new_conn = std::make_unique<MySqlConnection>(std::move(conn), timestamp);
        new_conn->conn.getSchema(schema_);
        {
            std::lock_guard guard(mutex_);
            pool_.push(std::move(new_conn));
        }

        std::cout << "MySql connection reconnect success" << std::endl;
        return true;
    }
    catch (const mysqlx::Error & e) {
        std::cerr << "MySql Reconnect failed, error is " << e.what() << std::endl;
        return false;
    }
}

std::shared_ptr<MySqlConnection> MySqlPool::Acquire()
{
    // 1. 等待池中有连接
    std::unique_lock lg_aquire(mutex_);
    cond_.wait(lg_aquire, [this] { return not pool_.empty(); });

    // 2. 从池中取出连接（独占所有权）
    std::unique_ptr<MySqlConnection> con_acquire(std::move(pool_.front()));
    pool_.pop();
    lg_aquire.unlock(); // 提前释放锁

    // 3. 构造一个 shared_ptr，带有自定义 deleter，回收时归还到池中
    auto deleter = [this](MySqlConnection* con_release) {
        std::unique_lock lg(this->mutex_);
        pool_.emplace(std::unique_ptr<MySqlConnection>(con_release));
        cond_.notify_one();
    };

    return std::shared_ptr<MySqlConnection>{con_acquire.release(), deleter};
}


}
