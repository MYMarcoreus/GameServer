#include "LogicInfoController.h"
#include "IPAddress.h"
#include "RWLock.h"

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

auto LogicInfoController::GetAllServerInfo() -> std::unordered_map<std::string, LogicServerInfoPtr>
{
    util::ReadLockGuard lg(mutex_);
    return logic_server_infos_;
}

void LogicInfoController::AddServerInfo(const std::string& server_name, net::IPAddressPtr addr)
{
    const auto info = std::make_shared<LogicServerInfo>(server_name, addr);
    util::WriteLockGuard lg(mutex_);
    logic_server_infos_.emplace(info->get_name(), info);
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
        [](const std::pair<std::string, LogicServerInfoPtr>& lhs, const std::pair<std::string, LogicServerInfoPtr>& rhs) {
            return lhs.second->get_room_cnt() < rhs.second->get_room_cnt();
        });

    return min_it->second;
}

}
