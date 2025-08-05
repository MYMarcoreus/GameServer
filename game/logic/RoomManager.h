#pragma once
#include <optional>
#include <unordered_map>

#include "Room.h"
#include "RWLock.h"

namespace yy::net
{
class EventLoopThreadPool;
class EventLoopThread;
}

namespace yy::app::logic
{

// 负责房间的增删查改，使用Actor模式
// todo 封装Actor框架
class RoomManager {
    static constexpr net::Milliseconds ROOM_TICK = std::chrono::duration_cast<net::Milliseconds>(std::chrono::duration<double>(1.0 / 128));
public:
    explicit RoomManager(net::EventLoop * base_loop);

    void AddRoom(protocol::app::RoomDetailData room_data, std::function<void(RoomPtr)> done);
    void RemoveRoom(ROOM_ID_t room_id, std::function<void(bool)> done);

    void FindRoomByUID(core::UID_t uid, std::function<void(RoomPtr)> done);
    void FindRoomByRoomID(ROOM_ID_t room_id, std::function<void(RoomPtr)> done);

    void AddPlayerToRoom(ROOM_ID_t room_id, core::UID_t uid, const core::UserConnectionPtr & userconn, std::function<void(RoomPtr)> done);

private:
    void FindRoomIDByUID(core::UID_t uid, std::function<void(std::optional<ROOM_ID_t>)> done);

    net::EventLoop *                                base_loop_;
    std::unordered_map<ROOM_ID_t, RoomPtr>          rooms_;
    std::unordered_map<core::UID_t, ROOM_ID_t>      uid_to_roomid_;
    util::RWMutex                                   mutex_;
    std::unique_ptr<net::EventLoopThreadPool>       work_threads;
};

}
