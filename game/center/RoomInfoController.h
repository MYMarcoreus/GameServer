#pragma once
#include <unordered_map>

#include "GameData.h"
#include "room_data.pb.h"
#include "account_data.pb.h"
#include "LogicInfoController.h"
#include "RWLock.h"
#include "util_functions.h"

namespace yy::app::center
{

using ROOM_ID_t = uint64_t;
using RoomDetailDataPtr  = std::shared_ptr<protocol::app::RoomDetailData>;

class RoomInfo {
    friend class RoomInfoController;
public:
    RoomInfo(const RoomDetailDataPtr& data, const LogicServerInfoPtr & server_name)
        : data_(data), server_info_(server_name)
    { }

    [[nodiscard]] ROOM_ID_t get_room_id() const { return data_.load(std::memory_order::acquire)->room_id(); }
    [[nodiscard]] auto get_room_data() const { return data_.load(std::memory_order::acquire); }
    [[nodiscard]] auto get_all_players() const { return data_.load(std::memory_order::acquire)->exist_player_datas(); }
    [[nodiscard]] size_t get_player_count() const { return data_.load(std::memory_order::acquire)->exist_player_datas_size(); }
    [[nodiscard]] LogicServerInfoPtr get_server_info() const { return server_info_; }

private:
    [[nodiscard]] bool AddPlayer(const protocol::app::AccountBaseData& account_data);
    [[nodiscard]] bool DelPlayer(core::UID_t uid);
    [[nodiscard]] bool FindPlayer(core::UID_t uid, protocol::app::AccountBaseData & out_player) const;

    std::atomic<RoomDetailDataPtr> data_;
    LogicServerInfoPtr server_info_;
};

using RoomInfoPtr = std::shared_ptr<RoomInfo>;

class RoomInfoController {
public:
    enum class AddPlayerResultCode {
        eSuccess,
        eRoomNotExist,
        eAlreadyJoined,
        eRoomFull,
        eInternalError
    };

    //Region 房间管理
    [[nodiscard]] RoomInfoPtr AddRoom(const protocol::app::RoomDetailData& room_data, LogicServerInfoPtr server_info, const protocol::app::AccountBaseData
                                      & owner_data);
    bool DelRoomIfEmpty(ROOM_ID_t room_id);
    [[nodiscard]] auto FindRoomByUID(core::UID_t uid) -> RoomInfoPtr;
    [[nodiscard]] auto FindRoomByRoomID(ROOM_ID_t room_id) -> RoomInfoPtr;
    //End

    [[nodiscard]] auto GetAllRoomData() -> std::vector<protocol::app::RoomDetailData>;
    [[nodiscard]] auto GetAllRoom() -> std::unordered_map<ROOM_ID_t, RoomInfoPtr> { return rooms_; }
    [[nodiscard]] size_t RoomCount() const { return rooms_.size(); }

    //Region 玩家管理
    [[nodiscard]] auto AddPlayer(ROOM_ID_t room_id, const protocol::app::AccountBaseData& account_data) -> std::pair<AddPlayerResultCode, RoomInfoPtr>;
    [[nodiscard]] auto DelPlayer(ROOM_ID_t room_id, core::UID_t uid) -> std::pair<bool, RoomInfoPtr>;
    [[nodiscard]] auto DelPlayer(core::UID_t uid) -> std::pair<bool, RoomInfoPtr>;
    [[nodiscard]] bool FindPlayer(core::UID_t uid, protocol::app::AccountBaseData & out_player);
    [[nodiscard]] bool FindPlayer(ROOM_ID_t room_id, core::UID_t uid, protocol::app::AccountBaseData & out_player);
    [[nodiscard]] bool IsJoinedAny(core::UID_t uid);
    [[nodiscard]] bool IsJoined(ROOM_ID_t room_id, core::UID_t uid);
    //End
private:
    [[nodiscard]] auto FindRoomIDByUID(core::UID_t uid) -> std::optional<ROOM_ID_t>;

    std::unordered_map<ROOM_ID_t, RoomInfoPtr>    rooms_;
    std::unordered_map<core::UID_t, ROOM_ID_t>    uid_to_roomid_;
    util::RWMutex                                 mutex_;
};

}
