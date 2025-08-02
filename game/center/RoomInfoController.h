#pragma once
#include <unordered_map>

#include "GameData.h"
#include "room_data.pb.h"
#include "RWLock.h"
#include "util_functions.h"

namespace yy::app::center
{

using ROOM_ID_t = uint64_t;
using RoomDetailDataPtr  = std::shared_ptr<protocol::app::RoomDetailData>;

struct RoomInfo
{
    RoomInfo(const RoomDetailDataPtr& data, const std::string& server_name)
        : data(data), server_name(server_name)
    { }

    RoomDetailDataPtr data;
    std::string server_name;
};
using RoomInfoPtr = std::shared_ptr<RoomInfo>;

class RoomInfoController {
public:
    //Region 房间管理
    RoomInfoPtr AddRoom(const protocol::app::RoomDetailData& room_data, const std::string& server_name);
    bool DelRoom(ROOM_ID_t room_id);
    auto FindRoomByUID(UID_t uid) -> RoomInfoPtr;
    auto FindRoomByRoomID(ROOM_ID_t room_id) -> RoomInfoPtr;
    //End

    //Region 玩家管理
    bool AddPlayer(const RoomInfoPtr& room, const protocol::app::AccountBaseData& account_data);
    bool DelPlayer(ROOM_ID_t room_id, UID_t uid);
    bool FindPlayer(UID_t uid, protocol::app::AccountBaseData & out_player);
    bool FindPlayer(ROOM_ID_t room_id, UID_t uid, protocol::app::AccountBaseData & out_player);
    //End
private:
    auto FindRoomIDByUID(UID_t uid) -> std::optional<ROOM_ID_t>;

    std::unordered_map<ROOM_ID_t, RoomInfoPtr>    rooms_;
    std::unordered_map<UID_t, ROOM_ID_t>          uid_to_roomid_;
    util::RWMutex                                 mutex_;
};

}
