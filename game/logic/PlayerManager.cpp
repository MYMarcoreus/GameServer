#include "PlayerManager.h"
#include "UserConnection.h"
#include "log.h"

namespace yy::app::logic
{
void PlayerManager::AddPlayer(const UID_t uid, const PlayerPtr& player)
{
    util::WriteLockGuard lg{mutex_};
    room_players_.emplace(uid, player);
}

PlayerPtr PlayerManager::RemovePlayer(const UID_t uid)
{
    util::WriteLockGuard lg{mutex_};
    const auto it = room_players_.find(uid);
    PlayerPtr player = nullptr;
    if (it != room_players_.end()) {
        player = it->second;
        room_players_.erase(it);
    }
    return player;
}

PlayerPtr PlayerManager::GetPlayer(const UID_t uid)
{
    util::ReadLockGuard lg{mutex_};
    const auto it = room_players_.find(uid);
    if (it == room_players_.end()) {
        return nullptr;
    }
    return it->second;
}

std::unordered_map<UID_t, PlayerPtr> PlayerManager::GetAllPlayers()
{
    std::unordered_map<UID_t, PlayerPtr> result;
    {
        util::ReadLockGuard lg{mutex_};
        result = room_players_;
    }
    return result;
}

bool PlayerManager::HasPlayer(const UID_t uid)
{
    util::ReadLockGuard lg{mutex_};
    return room_players_.contains(uid);
}

void PlayerManager::Broadcast(const PlayerPtr& from, const google::protobuf::Message& data)
{
    util::ReadLockGuard lg{mutex_};
    auto values = room_players_ | std::views::values;
    const std::vector to_all(values.begin(), values.end());
    lg.unlock();

    for(const auto & to: to_all)
    {
        if(to->GetUID() == from->GetUID())
            continue;

        YLOG_TRACE("Broadcast<{}>: from {} to {}, ", data.GetDescriptor()->full_name(), from->GetUID(), to->GetUID())
        to->GetConn()->SendUDP(data);
    }
}

void PlayerManager::Broadcast(const PlayerPtr& from, const core::MessagePtr& data)
{
    if (from and data) {
        this->Broadcast(from, *data);
    }
}
}
