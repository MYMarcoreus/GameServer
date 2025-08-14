#pragma once
#include <optional>
#include <string>
#include "account.pb.h"
#include "AccountData.h"
#include "Singleton.h"

namespace yy::net { class EventLoop; }
namespace yy::core::redis { class RedisClient; }

namespace yy::app::center
{

class CenterRedisDAO final : public Singleton<CenterRedisDAO>{
    SINGLETON_NECESSITY(CenterRedisDAO);
    constexpr static std::string USR_TKN_field = "usr_token";
    constexpr static std::string SCN_TKN_field = "scn_token";
public:
    explicit CenterRedisDAO();
    ~CenterRedisDAO() override;
    void Start(net::EventLoop* loop);

    auto SetSceneTokenWithExpire(uint64_t uid, const std::string&, std::chrono::seconds) -> bool;

private:
    core::redis::RedisClient& redis_client_;
};

}
