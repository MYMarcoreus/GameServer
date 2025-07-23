#pragma once
#include "Player.h"
#include "RWLock.h"

namespace yy::app::logic
{

// 管理一个房间内的玩家
class Room {
public:
    void AddPlayer(UID_t uid, const PlayerPtr & player);

    PlayerPtr RemovePlayer(UID_t uid);
    PlayerPtr GetPlayer(UID_t uid);
    auto GetAllPlayers() -> std::unordered_map<UID_t, PlayerPtr>;
    bool HasPlayer(UID_t uid);
    void Broadcast(const PlayerPtr& from, const google::protobuf::Message &data);
    void Broadcast(const PlayerPtr& from, const core::MessagePtr &data);

private:
    UID_t                                   room_owner_;
    std::unordered_map<UID_t, PlayerPtr>    room_players_;
    util::RWMutex                           mutex_;

};

}
