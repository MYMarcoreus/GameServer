#pragma once
#include <shared_mutex>
#include <unordered_map>

#include "Room.h"
#include "ActorSystem.h"

namespace yy::app::logic
{

using RoomRef = core::actor::ActorRef<Room>;

// 负责房间的增删查改，使用Actor模式（基于 core::actor::ActorSystem 封装）。
// 线程模型：本类自身线程安全（业务索引由读写锁保护），可在任意线程调用；
// 房间私有状态仍只在房间绑定的线程内访问。
class RoomManager {
public:
    explicit RoomManager(net::EventLoop * base_loop);

    /// @brief 创建房间；room_id 冲突时返回空句柄
    RoomRef AddRoom(protocol::app::RoomDetailData room_data);

    /// @brief 删除房间；返回是否成功
    bool RemoveRoom(ROOM_ID_t room_id);

    RoomRef FindRoomByUID(core::UID_t uid);
    RoomRef FindRoomByRoomID(ROOM_ID_t room_id);

    /// @brief 加入房间；返回的 Future<bool> 在 Room 线程内确认真正加入成功与否
    core::actor::Future<bool> AddPlayerToRoom(ROOM_ID_t room_id, protocol::app::AccountBaseData account_data, const UserConnectionPtr & userconn);

    /// @brief 监控指标：当前注册的房间数 / 在线玩家数
    std::size_t GetRoomCount();
    std::size_t GetOnlinePlayerCount();

private:
    void AddUIDIndexLocked(core::UID_t uid, ROOM_ID_t room_id);
    void RemoveUIDIndexLocked(core::UID_t uid);
    void RemoveRoomIndexLocked(ROOM_ID_t room_id);

    net::EventLoop *                                base_loop_;
    std::unordered_map<ROOM_ID_t, RoomRef>          rooms_;
    std::unordered_map<core::UID_t, ROOM_ID_t>      uid_to_roomid_;
    std::shared_mutex                               mutex_;
    std::unique_ptr<core::actor::ActorSystem>       actor_system_;
};

}
