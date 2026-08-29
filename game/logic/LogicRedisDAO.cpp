#include "LogicRedisDAO.h"

#include <format>
#include <stdexcept>

#include "RedisClient.h"
#include "log.h"

using namespace std::chrono_literals;

namespace yy::app::logic
{

LogicRedisDAO::LogicRedisDAO():
    redis_client_(core::redis::RedisClient::Instance())
{
}

LogicRedisDAO::~LogicRedisDAO()
{
}

void LogicRedisDAO::Start(net::EventLoop* loop)
{
    redis_client_.Start(loop, 5);
    redis_pool_ = std::make_unique<net::ThreadPool>("RedisPool");
    redis_pool_->Start(loop, 1); //! 单线程串行执行 Redis 阻塞调用，避免并发与阻塞 IO 线程
}

auto LogicRedisDAO::GetAndDelSceneToken(const uint64_t uid) -> std::optional<std::string>
{
    const auto key = std::format("{}_{}", SCN_TKN_field, std::to_string(uid));
    auto scene_token = redis_client_.Get(key);
    if (scene_token.has_value()) {
        redis_client_.Del(key);
    }
    return scene_token;
}

auto LogicRedisDAO::GetUserTokenAndRefreshEx(const uint64_t uid) const -> std::optional<std::string>
{
    const auto uid_str = std::to_string(uid);
    const auto key = std::format("{}_{}", USR_TKN_field, uid_str);
    const auto rst = redis_client_.GetAndRefreshEx(key, 1800s);
    if (rst) {
        return *rst;
    }
    // Redis 故障时抛出异常，保持“登录失败”语义（FetchLoginDataAsync 的 onError 会处理）；
    // key 不存在（无 token）则返回 nullopt。
    if (rst.error() == core::redis::RedisError::kError) {
        YLOG_ERROR("LogicRedisDAO::GetUserTokenAndRefreshEx Redis 错误：{}", rst.error().message())
        throw std::runtime_error(rst.error().message());
    }
    return std::nullopt;
}

auto LogicRedisDAO::GetAccountData(const uint64_t uid) -> std::optional<account::AccountData>
{
    const std::string uid_str = std::to_string(uid);
    const auto key = std::format("{}_{}", ACT_field, uid_str);

    if (not redis_client_.HasHashKey(key)) {
        redis_client_.Del(key);
    }

    const auto usr = redis_client_.HGet(key, USR_field);

    if (usr.has_value()) {
        account::AccountData data;
        data.username = usr.value();
        data.uid = uid;
        return data;
    }
    return std::nullopt;
}

core::actor::Future<LoginData> LogicRedisDAO::FetchLoginDataAsync(const uint64_t uid)
{
    auto promise = std::make_shared<core::actor::Promise<LoginData>>();
    auto future = promise->get_future();
    redis_pool_->PushTask([this, uid, promise] {
        try {
            LoginData data;
            data.scene_token = GetAndDelSceneToken(uid);
            data.user_token  = GetUserTokenAndRefreshEx(uid);
            data.account     = GetAccountData(uid);
            promise->set_value(std::move(data));
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
    });
    return future;
}
}
