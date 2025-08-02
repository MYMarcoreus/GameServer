#include "LogicInfoController.h"

#include <atomic>
#include "IPAddress.h"

namespace yy::app::center
{
LogicInfoController::LogicInfoController(const std::string& logic_service_name) :
    logic_service_name_(logic_service_name)
{
}

auto LogicInfoController::FindServerInfo(const std::string& name) -> LogicServerInfoPtr
{
    util::ReadLockGuard lg(mutex_);
    const auto it = logic_server_infos_.find(name);
    return it == logic_server_infos_.end() ? nullptr : it->second;
}

bool LogicInfoController::AddServerInfo(std::string ip, uint16_t port, const std::string& server_name)
{
    if (FindServerInfo(server_name) != nullptr) {
        return false;
    }
    auto info = std::make_shared<LogicServerInfo>(ip, port, server_name, 0);
    util::WriteLockGuard lg(mutex_);
    auto [it, is_insert] = logic_server_infos_.emplace(info->get_name(), info);
    return is_insert;
}

bool LogicInfoController::RemoveServerInfo(const std::string& name)
{
    util::WriteLockGuard lg(mutex_);
    return logic_server_infos_.erase(name) > 0;
}

LogicServerInfoPtr LogicInfoController::GetMinPlayerServerInfo()
{
    util::ReadLockGuard lg(mutex_);
    if (logic_server_infos_.empty()) {
        return nullptr;
    }

    const auto min_it = std::min_element(
        logic_server_infos_.begin(), logic_server_infos_.end(),
        [](const auto& lhs, const auto& rhs) {
            return lhs.second->get_player_cnt() < rhs.second->get_player_cnt();
        });

    return min_it->second;
}

}
