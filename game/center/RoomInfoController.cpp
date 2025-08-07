#include "RoomInfoController.h"

#include "log.h"

using namespace yy::core;

namespace yy::app::center
{

using namespace yy::protocol::app;

bool RoomInfo::AddPlayer(const AccountBaseData& account_data)
{
    while (true) {
        auto data_ptr = data_.load(std::memory_order::acquire);
        if (!data_ptr) return false;

        // 创建副本并添加玩家
        if (data_ptr->exist_player_datas_size() >= data_ptr->capacity())
            return false;

        const auto new_data = std::make_shared<RoomDetailData>(*data_ptr);
        new_data->add_exist_player_datas()->CopyFrom(account_data);

        // CAS 尝试更新
        if (data_.compare_exchange_weak(data_ptr, new_data,
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
        auto data_ptr = data_.load(std::memory_order::acquire);
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
        if (data_.compare_exchange_weak(data_ptr, new_data,
            std::memory_order_release, std::memory_order_relaxed))
        {
            return true;
        }

        // 否则重新 load 并重试
    }

    return false;
}

bool RoomInfo::FindPlayer(const UID_t uid, AccountBaseData& out_player) const
{
    const auto room_data = data_.load(std::memory_order::acquire);
    if (!room_data) return false;

    for (const auto& player : room_data->exist_player_datas()) {
        if (player.uid() == uid) {
            out_player = player;
            return true;
        }
    }
    return false;
}

RoomInfoPtr RoomInfoController::AddRoom(const RoomDetailData& room_data, LogicServerInfoPtr server_info, const AccountBaseData& owner_data)
{
    const auto room_data_ptr = std::make_shared<RoomDetailData>(room_data);
    auto room_info = std::make_shared<RoomInfo>(room_data_ptr, server_info);
    {
        util::WriteLockGuard lg(mutex_);
        uid_to_roomid_.emplace(room_data_ptr->owner_uid(), room_data_ptr->room_id());
        rooms_.emplace(room_data_ptr->room_id(), room_info);
    }
    //! 中央服务器添加玩家
    const auto is_added = room_info->AddPlayer(owner_data);
    if (not is_added) {
        DelRoomIfEmpty(room_info->get_room_id());
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

auto RoomInfoController::AddPlayer(const ROOM_ID_t room_id, const AccountBaseData& account_data)
    -> std::pair<AddPlayerResultCode, RoomInfoPtr>
{
    const auto room = FindRoomByRoomID(room_id);
    if (!room) return {AddPlayerResultCode::eRoomNotExist, nullptr};

    {
        util::ReadLockGuard lg(mutex_);
        if (uid_to_roomid_.contains(account_data.uid())) {
            return {AddPlayerResultCode::eAlreadyJoined, room};
        }
    }

    // 注意：先尝试加入房间，成功后再更新映射，避免脏数据
    const bool ok = room->AddPlayer(account_data);
    if (!ok) {
        return {AddPlayerResultCode::eRoomFull, room};
    }

    {
        util::WriteLockGuard lg(mutex_);
        uid_to_roomid_[account_data.uid()] = room_id;
    }

    return {AddPlayerResultCode::eSuccess, room};
}


auto RoomInfoController::DelPlayer(const ROOM_ID_t room_id, const UID_t uid) -> std::pair<bool, RoomInfoPtr>
{
    const auto room = FindRoomByRoomID(room_id);
    if (!room) return {false, nullptr};

    {
        util::WriteLockGuard lg(mutex_);
        uid_to_roomid_.erase(uid);
    }

    return {room->DelPlayer(uid), room};
}

auto RoomInfoController::DelPlayer(const UID_t uid) -> std::pair<bool, RoomInfoPtr>
{
    const auto room = FindRoomByUID(uid);
    if (!room) return {false, nullptr};

    {
        util::WriteLockGuard lg(mutex_);
        uid_to_roomid_.erase(uid);
    }

    return {room->DelPlayer(uid), room};
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
