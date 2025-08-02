#include "RoomManager.h"

#include <ranges>

#include "EventLoop.h"
#include "util_functions.h"

#include "EventLoopThread.h"


namespace yy::app::logic
{
RoomManager::RoomManager()
{
    (void)0;
}

RoomPtr RoomManager::AddRoom(EventLoop* loop, RoomDetailData room_data)
{
    // 初始化房间对象，开启Update
    auto room = std::make_shared<Room>(loop, room_data);
    room->Init(ROOM_TICK);

    // 加入房间列表
    {
        util::WriteLockGuard lg(mutex_);
        rooms_.emplace(room->get_id(), room);
        uid_to_roomid_.emplace(room->get_owner_uid(), room->get_id());
    }
    return room;
}

bool RoomManager::RemoveRoom(const ROOM_ID_t room_id)
{
    const auto room = FindRoomByRoomID(room_id);
    if (room == nullptr) {
        return false;
    }

    util::WriteLockGuard lg(mutex_);
    for (const auto& player_uid : room->GetAllPlayers() | std::views::keys) {
        uid_to_roomid_.erase(player_uid);
    }
    return rooms_.erase(room_id) > 0;
}

std::optional<ROOM_ID_t> RoomManager::FindRoomIDByUID(const UID_t uid)
{
    util::ReadLockGuard lg(mutex_);
    const auto it_room_id = uid_to_roomid_.find(uid);
    if (it_room_id == uid_to_roomid_.end()) {
        return std::nullopt;
    }
    return it_room_id->second;
}

RoomPtr RoomManager::FindRoomByRoomID(const ROOM_ID_t room_id)
{
    util::ReadLockGuard lg(mutex_);
    const auto it = rooms_.find(room_id);
    return it != rooms_.end() ? it->second : nullptr;
}

RoomPtr RoomManager::FindRoomByUID(const UID_t uid)
{
    const auto room_id = FindRoomIDByUID(uid);
    return room_id ? FindRoomByRoomID(room_id.value()) : nullptr;
}

PlayerPtr RoomManager::FindPlayer(const UID_t uid)
{
    const auto room = FindRoomByUID(uid);
    return room ? room->FindPlayer(uid) : nullptr;
}

PlayerPtr RoomManager::FindPlayer(const ROOM_ID_t room_id, const UID_t uid)
{
    const auto room = FindRoomByRoomID(room_id);
    return room ? room->FindPlayer(uid) : nullptr;
}

}
