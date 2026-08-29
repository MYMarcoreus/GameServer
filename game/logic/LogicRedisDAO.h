#pragma once
#include <memory>
#include <optional>
#include <string>

#include "account.pb.h"
#include "AccountData.h"
#include "Future.h"
#include "Singleton.h"
#include "ThreadPool.h"

namespace yy::net { class EventLoop; }
namespace yy::core::redis { class RedisClient; }

namespace yy::app::logic
{

/// @brief 一次登录所需的全部 Redis 数据
struct LoginData {
    std::optional<std::string> scene_token;
    std::optional<std::string> user_token;
    std::optional<account::AccountData> account;
};

class LogicRedisDAO final : public Singleton<LogicRedisDAO>{
    SINGLETON_NECESSITY(LogicRedisDAO);
    constexpr static std::string USR_TKN_field = "usr_token";
    constexpr static std::string SCN_TKN_field = "scn_token";

    constexpr static std::string PWD_field = "password";
    constexpr static std::string UID_field = "uid";
    constexpr static std::string USR_field = "username";
    constexpr static std::string ACT_field = "account";
public:
    void Start(net::EventLoop* loop);

    auto GetAndDelSceneToken(uint64_t uid) -> std::optional<std::string>;
    auto GetUserTokenAndRefreshEx(uint64_t uid) const -> std::optional<std::string>;
    auto GetAccountData(uint64_t uid) -> std::optional<account::AccountData>;

    /// @brief 异步获取登录数据（在 Redis 工作线程执行，避免阻塞 IO 线程）
    core::actor::Future<LoginData> FetchLoginDataAsync(uint64_t uid);

private:
    explicit LogicRedisDAO();
    ~LogicRedisDAO() override;

    core::redis::RedisClient& redis_client_;
    std::unique_ptr<net::ThreadPool> redis_pool_;
};

}
