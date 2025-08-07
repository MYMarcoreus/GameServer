#pragma once
#include <atomic>
#include <string>
#include <unordered_map>

#include "IPAddress.h"
#include "net_definations.h"
#include "RWLock.h"

namespace yy::core::zk
{
class ZkServiceClient;
}

namespace yy::app::center
{

struct LogicServerInfo {
    LogicServerInfo(const std::string& name, const net::IPAddressPtr& addr)
        : name(name), addr(addr)
    {
    }

    void AddPlayerCnt() { player_cnt.fetch_add(1, std::memory_order::relaxed); }
    void SubPlayerCnt() { player_cnt.fetch_sub(1, std::memory_order::relaxed); }

    [[nodiscard]] std::string get_name() const { return name; }
    [[nodiscard]] std::string get_ip() const { return addr->GetIPStr(); }
    [[nodiscard]] uint16_t get_port() const { return addr->GetPort(); }
    [[nodiscard]] uint64_t get_player_cnt() const { return player_cnt; }

private:
    std::string          name{};
    net::IPAddressPtr    addr{};
    std::atomic_int   player_cnt{0};
};
using LogicServerInfoPtr = std::shared_ptr<LogicServerInfo>;

class LogicInfoController {
    friend class CenterRpcServiceImpl;
public:
    explicit LogicInfoController(const std::string& logic_service_name);

    void AddPlayerCnt() {  }
    void SubPlayerCnt() {  }

    auto FindServerInfo(const std::string& name) -> LogicServerInfoPtr;
    void AddServerInfo(const std::string& server_name, net::IPAddressPtr addr);
    bool RemoveServerInfo(const std::string& name);

    LogicServerInfoPtr GetMinPlayerServerInfo();

private:
    std::unordered_map<std::string, LogicServerInfoPtr>   logic_server_infos_;
    std::string                                           logic_service_name_;
    util::RWMutex                                         mutex_;
};

}
