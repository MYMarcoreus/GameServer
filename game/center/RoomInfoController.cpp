#include "RoomInfoController.h"

#include "log.h"

using namespace yy::core;

namespace yy::app::center
{

using namespace yy::protocol::app;

bool RoomInfo::AddPlayer(const AccountBaseData& account_data)
{
    while (true) {
        auto data_ptr = data.load(std::memory_order::acquire);
        if (!data_ptr) return false;

        // 创建副本并添加玩家
        const auto new_data = std::make_shared<RoomDetailData>(*data_ptr);
        new_data->add_exist_player_datas()->CopyFrom(account_data);

        // CAS 尝试更新
        if (data.compare_exchange_weak(data_ptr, new_data,
                                       std::memory_order_release,   // 能更新，写入
                                       std::memory_order_relaxed    // 不能更新，无需写入
        )){
            return true;
        }

        // 如果失败，说明 data_ptr 被修改，进入下一轮重试
    }

    return false; // 实际不会到这里
}

bool RoomInfo::DelPlayer(const UID_t uid)
{
    while (true) {
        auto data_ptr = data.load(std::memory_order::acquire);
        if (!data_ptr) return false;

        // 删除玩家
        const auto& repeated = data_ptr->exist_player_datas();
        int idx = -1;
        for (int i = repeated.size() - 1; i >= 0; --i) {
            if (repeated.Get(i).uid() == uid) {
                idx = i;
                break;
            }
        }
        if (idx == -1) return false; // 玩家不存在

        // 创建新副本并删除玩家
        const auto new_data = std::make_shared<RoomDetailData>(*data_ptr);
        new_data->mutable_exist_player_datas()->DeleteSubrange(idx, 1);

        // CAS 尝试更新
        if (data.compare_exchange_weak(data_ptr, new_data,
            std::memory_order_release, std::memory_order_relaxed))
        {
            return true;
        }

        // 否则重新 load 并重试
    }

    return false;
}

bool RoomInfo::FindPlayer(const UID_t uid, AccountBaseData& out_player)
{
    const auto room_data = data.load(std::memory_order::acquire);
    if (!room_data) return false;

    for (const auto& player : room_data->exist_player_datas()) {
        if (player.uid() == uid) {
            out_player = player;
            return true;
        }
    }
    return false;
}

RoomInfoPtr RoomInfoController::AddRoom(const RoomDetailData& room_data, LogicServerInfoPtr server_info)
{
    const auto room_data_ptr = std::make_shared<RoomDetailData>(room_data);
    auto room_info = std::make_shared<RoomInfo>(room_data_ptr, server_info);
    {
        util::WriteLockGuard lg(mutex_);
        uid_to_roomid_.emplace(room_data_ptr->owner_uid(), room_data_ptr->room_id());
        rooms_.emplace(room_data_ptr->room_id(), room_info);
    }

    return room_info;
}

bool RoomInfoController::DelRoomIfEmpty(const ROOM_ID_t room_id)
{
    const auto room = FindRoomByRoomID(room_id);
    if (!room) return false;

    const auto room_data = room->get_room_data();
    if (!room_data or !room_data->exist_player_datas().empty()) {
        return false;
    }

    util::WriteLockGuard lg(mutex_);
    return rooms_.erase(room_id) > 0;
}

bool RoomInfoController::AddPlayer(const ROOM_ID_t room_id, const AccountBaseData& account_data) {
    const auto room = FindRoomByRoomID(room_id);
    if (!room) return false;
    return  room->AddPlayer(account_data);
}



bool RoomInfoController::DelPlayer(const ROOM_ID_t room_id, const UID_t uid) {
    const auto room = FindRoomByRoomID(room_id);
    if (!room) return false;
    return room->DelPlayer(uid);
}

bool RoomInfoController::DelPlayer(const UID_t uid)
{
    const auto room = FindRoomByUID(uid);
    if (!room) return false;
    return room->DelPlayer(uid);
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

auto RoomInfoController::GetAllRoomData() -> std::vector<RoomDetailData>
{
    std::vector<RoomDetailData> result;
    result.reserve(rooms_.size());  // 提前分配，避免多次扩容
    for (const auto& room_ptr : rooms_ | std::views::values) {
        if (room_ptr) {
            result.push_back(*room_ptr->get_room_data());
        }
    }

    return result;
}

bool RoomInfoController::FindPlayer(const UID_t uid, AccountBaseData& out_player)
{
    const auto room = FindRoomByUID(uid);
    if (!room) return false;

    return room->FindPlayer(uid, out_player);
}

bool RoomInfoController::FindPlayer(const ROOM_ID_t room_id, const UID_t uid, AccountBaseData& out_player)
{
    const auto room = FindRoomByRoomID(room_id);
    if (!room) return false;

    return room->FindPlayer(uid, out_player);
}

bool RoomInfoController::IsJoinedAny(const UID_t uid)
{
    util::ReadLockGuard lg(mutex_);
    return uid_to_roomid_.contains(uid);
}

bool RoomInfoController::IsJoined(const ROOM_ID_t room_id, const UID_t uid)
{
    util::ReadLockGuard lg(mutex_);
    const auto it = uid_to_roomid_.find(uid);
    return it != uid_to_roomid_.end() ? it->second == room_id : false;
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
