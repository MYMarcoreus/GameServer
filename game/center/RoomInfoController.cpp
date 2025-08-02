#include "RoomInfoController.h"

namespace yy::app::center
{

using namespace yy::protocol::app;

RoomInfoPtr RoomInfoController::AddRoom(const RoomDetailData& room_data, const std::string& server_name)
{
    const auto room_data_ptr = std::make_shared<RoomDetailData>(room_data);
    auto room_info = std::make_shared<RoomInfo>(room_data_ptr, server_name);
    const RoomBriefData * const room_breif_data = room_data_ptr->mutable_breif_data();
    {
        util::WriteLockGuard lg(mutex_);
        rooms_.emplace(room_breif_data->room_id(), room_info);
        uid_to_roomid_.emplace(room_breif_data->owner_uid(), room_breif_data->room_id());
    }

    return room_info;
}

bool RoomInfoController::DelRoom(const ROOM_ID_t room_id)
{
    const auto room = FindRoomByRoomID(room_id);
    if (room == nullptr) return false;

    util::WriteLockGuard lg(mutex_);
    for (const AccountBaseData & player : room->data->exist_player_datas()) {
        uid_to_roomid_.erase(player.uid());
    }
    return rooms_.erase(room_id) > 0;
}

bool RoomInfoController::AddPlayer(const RoomInfoPtr& room, const AccountBaseData& account_data)
{
    util::WriteLockGuard lg(mutex_);
    if (room == nullptr)
        return false;
    room->data->add_exist_player_datas()->CopyFrom(account_data);
    const auto brief = room->data->mutable_breif_data();
    brief->set_size(brief->size() + 1);
    return true;
}

bool RoomInfoController::DelPlayer(const ROOM_ID_t room_id, const UID_t uid)
{
    util::WriteLockGuard lg(mutex_);
    const auto room = FindRoomByRoomID(room_id);
    if (room == nullptr or room->data == nullptr)
        return false;

    auto* repeated = room->data->mutable_exist_player_datas();
    for (int i = repeated->size() - 1; i >= 0; --i) {
        if (repeated->Get(i).uid() == uid) {
            repeated->DeleteSubrange(i, 1);
            return true;
        }
    }
    const auto brief = room->data->mutable_breif_data();
    if (brief->size() > 0) {
        brief->set_size(brief->size() + 1);
    }
    return false;
}

auto RoomInfoController::FindRoomByUID(const UID_t uid) -> RoomInfoPtr
{
    const auto room_id = FindRoomIDByUID(uid);
    return room_id ? FindRoomByRoomID(room_id.value()) : nullptr;
}

auto RoomInfoController::FindRoomByRoomID(const ROOM_ID_t room_id) -> RoomInfoPtr
{
    util::ReadLockGuard lg(mutex_);
    const auto it = rooms_.find(room_id);
    return it != rooms_.end() ? it->second : nullptr;
}

bool RoomInfoController::FindPlayer(const UID_t uid, AccountBaseData & out_player)
{
    const auto room = FindRoomByUID(uid);
    if (room == nullptr) return false;

    for (const AccountBaseData & player : room->data->exist_player_datas()) {
        if (player.uid() == uid) {
            out_player = player;
            return true;
        }
    }
    return false;
}

bool RoomInfoController::FindPlayer(const ROOM_ID_t room_id, const UID_t uid, AccountBaseData& out_player)
{
    const auto room = FindRoomByRoomID(room_id);
    if (room == nullptr) return false;

    for (const AccountBaseData & player : room->data->exist_player_datas()) {
        if (player.uid() == uid) {
            out_player = player;
            return true;
        }
    }
    return false;
}




auto RoomInfoController::FindRoomIDByUID(const UID_t uid) -> std::optional<ROOM_ID_t>
{
    util::ReadLockGuard lg(mutex_);
    const auto it_room_id = uid_to_roomid_.find(uid);
    if (it_room_id == uid_to_roomid_.end()) {
        return std::nullopt;
    }
    return it_room_id->second;
}
}
