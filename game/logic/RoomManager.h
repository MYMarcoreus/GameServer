#pragma once
#include <optional>
#include <unordered_map>

#include "Room.h"
#include "RWLock.h"

namespace yy::net
{
class EventLoopThread;
}

namespace yy::app::logic
{

// 负责房间的增删查改，使用读写锁
class RoomManager {
    static constexpr Milliseconds ROOM_TICK = std::chrono::duration_cast<Milliseconds>(std::chrono::duration<double>(1.0 / 128));
public:
    RoomManager();

    RoomPtr AddRoom(EventLoop* loop, RoomDetailData room_data);
    bool RemoveRoom(ROOM_ID_t room_id);

    auto FindRoomByUID(UID_t uid) -> RoomPtr;
    auto FindRoomByRoomID(ROOM_ID_t room_id) -> RoomPtr;
    auto FindPlayer(UID_t uid) -> PlayerPtr;
    auto FindPlayer(ROOM_ID_t room_id, UID_t uid) -> PlayerPtr;

private:
    auto FindRoomIDByUID(UID_t uid) -> std::optional<ROOM_ID_t>;

    std::unordered_map<ROOM_ID_t, RoomPtr>          rooms_;
    std::unordered_map<UID_t, ROOM_ID_t>            uid_to_roomid_;
    util::RWMutex                                   mutex_;
};

}
