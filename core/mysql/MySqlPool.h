#pragma once

#include <mysqlx/xdevapi.h> // 这个可能需要ZkServiceManager.h在MySqlClient.h之前导入
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>
#include <memory>
#include <chrono>
#include "net_definations.h"


namespace yy::core::mysql
{


class MySqlConnection {
public:
	MySqlConnection(mysqlx::Session && sess, const int64_t lasttime)
		: conn(std::move(sess)), last_oper_time(lasttime) {}

	mysqlx::Session conn;
	int64_t last_oper_time;
};

class MySqlPool {
public:
	MySqlPool(net::EventLoop * loop, const std::string& ip, int port, const std::string& user, const std::string& pwd, const std::string& schema, size_t poolSize);

	~MySqlPool();

    // 获取 RedisConnType 连接（RAII 封装，自动归还）
	std::shared_ptr<MySqlConnection> Acquire();

private:
	bool reconnect(long long timestamp);
	void check_connection();

	net::EventLoop * loop_;
	std::string ip_;
	int port_;
	std::string user_;
	std::string pass_;
	std::string schema_;
	size_t max_size_;
	std::queue<std::unique_ptr<MySqlConnection>> pool_;
	std::mutex mutex_;
	std::condition_variable cond_;
	std::atomic<bool> is_stop_;
	std::atomic<int> _fail_count;
	net::TimerID checker_timer_id_;
};


}
