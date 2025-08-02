#pragma once
#include <format>
#include <string>
#include <unordered_map>
#include "net_definations.h"
#include "ZkServiceClient.h"

namespace yy::core::zk
{
class ZkServiceClient;
}

namespace yy::app::center
{

struct LogicServerInfo {
    LogicServerInfo(const std::string& ip, const uint16_t port, const std::string& name, const uint64_t player_cnt = 0)
        : ip(ip),
          port(port),
          name(name),
          player_cnt(player_cnt)
    {
    }

    void AddPlayerCnt() { player_cnt.fetch_add(1, std::memory_order::relaxed); }

    [[nodiscard]] std::string get_ip() const { return ip; }
    [[nodiscard]] uint16_t get_port() const { return port; }
    [[nodiscard]] std::string get_name() const { return name; }
    [[nodiscard]] uint64_t get_player_cnt() const { return player_cnt; }

private:
    std::string          ip{};
    uint16_t             port{};
    std::string          name{};
    std::atomic_size_t   player_cnt{0};
};
using LogicServerInfoPtr = std::shared_ptr<LogicServerInfo>;

class LogicInfoController {
public:
    explicit LogicInfoController(const std::string& logic_service_name);

    auto FindServerInfo(const std::string& name) -> LogicServerInfoPtr;
    bool AddServerInfo(std::string ip, uint16_t port, const std::string& server_name);
    bool RemoveServerInfo(const std::string& name);

    LogicServerInfoPtr GetMinPlayerServerInfo();

private:
    std::unordered_map<std::string, LogicServerInfoPtr>   logic_server_infos_;
    std::string                                           logic_service_name_;
    util::RWMutex                                         mutex_;
};

}
