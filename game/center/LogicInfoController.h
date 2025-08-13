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
        : name_(name), addr_(addr), room_cnt_(0)
    {
    }

    void AddRoomCnt()
    {
        room_cnt_.fetch_add(1, std::memory_order::release);
        (void)0;
    }
    void SubRoomCnt()
    {
        room_cnt_.fetch_sub(1, std::memory_order::release);
        (void)0;
    }

    [[nodiscard]] std::string get_name() const { return name_; }
    [[nodiscard]] std::string get_ip() const { return addr_->GetIPStr(); }
    [[nodiscard]] uint16_t get_port() const { return addr_->GetPort(); }
    [[nodiscard]] uint64_t get_room_cnt() const { return room_cnt_.load(std::memory_order::acquire); }

private:
    std::string         name_;
    net::IPAddressPtr   addr_;
    std::atomic_int     room_cnt_;
};
using LogicServerInfoPtr = std::shared_ptr<LogicServerInfo>;

class LogicInfoController {
    friend class CenterServiceRpc_Impl;
public:
    explicit LogicInfoController(const std::string& logic_service_name);

    auto FindServerInfo(const std::string& name) -> LogicServerInfoPtr;
    auto GetAllServerInfo() -> std::unordered_map<std::string, LogicServerInfoPtr>;
    void AddServerInfo(const std::string& server_name, net::IPAddressPtr addr);
    bool RemoveServerInfo(const std::string& name);

    LogicServerInfoPtr GetMinPlayerServerInfo();

private:
    std::unordered_map<std::string, LogicServerInfoPtr>   logic_server_infos_;
    std::string                                           logic_service_name_;
    util::RWMutex                                         mutex_;
};

}
