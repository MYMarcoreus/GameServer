#include "RoomManager.h"
#include "log.h"
#include "AppXmlConfig.h"
#include "EventLoop.h"

using namespace yy::net;
using namespace yy::core;
using namespace yy::app;
using namespace yy::protocol::app;

namespace yy::app::logic
{
RoomManager::RoomManager(EventLoop * base_loop): base_loop_(base_loop)
{
    actor_system_ = std::make_unique<core::actor::ActorSystem>(base_loop_, static_cast<int>(config::g_app_config->GetValue().work_thread_num()));

    //! 轻量监控：每 10s 输出一条汇总
    base_loop_->RunEvery(10s, [this]() {
        YLOG_INFO("[RoomManager] 在线房间数: {}, 在线玩家数: {}", GetRoomCount(), GetOnlinePlayerCount());
    });
}

std::size_t RoomManager::GetRoomCount()
{
    std::shared_lock lock{mutex_};
    return rooms_.size();
}

std::size_t RoomManager::GetOnlinePlayerCount()
{
    std::shared_lock lock{mutex_};
    return uid_to_roomid_.size();
}

void RoomManager::AddUIDIndexLocked(const UID_t uid, const ROOM_ID_t room_id)
{
    uid_to_roomid_[uid] = room_id;
}

void RoomManager::RemoveUIDIndexLocked(const UID_t uid)
{
    uid_to_roomid_.erase(uid);
}

void RoomManager::RemoveRoomIndexLocked(const ROOM_ID_t room_id)
{
    rooms_.erase(room_id);
    for (auto it = uid_to_roomid_.begin(); it != uid_to_roomid_.end();) {
        if (it->second == room_id) it = uid_to_roomid_.erase(it);
        else ++it;
    }
}

RoomRef RoomManager::AddRoom(RoomDetailData room_data)
{
    const ROOM_ID_t room_id = room_data.room_id();
    const UID_t owner_uid = room_data.owner_uid();

    bool exists = false;
    {
        std::shared_lock lock{mutex_};
        exists = rooms_.contains(room_id);
    }
    if (exists) {
        return RoomRef{}; //! 房间已存在
    }

    // 分配线程、注册（ActorSystem 线程安全，可在任意线程调用）
    auto ref = actor_system_->Spawn<Room>(room_data,
        [this](UID_t uid) { //! 玩家被移除：清理 uid 索引
            std::unique_lock lock{mutex_};
            RemoveUIDIndexLocked(uid);
        },
        [this](ROOM_ID_t room_id) { //! 房间停止：清理房间与 uid 索引
            std::unique_lock lock{mutex_};
            RemoveRoomIndexLocked(room_id);
        });

    {
        std::unique_lock lock{mutex_};
        rooms_[room_id] = ref;
        AddUIDIndexLocked(owner_uid, room_id);
    }
    return ref;
}

bool RoomManager::RemoveRoom(const ROOM_ID_t room_id)
{
    RoomRef ref;
    bool found = false;
    {
        std::unique_lock lock{mutex_};
        const auto it = rooms_.find(room_id);
        if (it != rooms_.end()) {
            ref = it->second;
            found = true;
            RemoveRoomIndexLocked(room_id);
        }
    }

    if (found) {
        actor_system_->Stop(ref.GetID()); //! 用 ActorID 停止（业务 room_id 不是 ActorID）
    }
    return found;
}

RoomRef RoomManager::FindRoomByRoomID(const ROOM_ID_t room_id)
{
    std::shared_lock lock{mutex_};
    const auto it = rooms_.find(room_id);
    return it != rooms_.end() ? it->second : RoomRef{};
}

RoomRef RoomManager::FindRoomByUID(const UID_t uid)
{
    std::shared_lock lock{mutex_};
    const auto it_room_id = uid_to_roomid_.find(uid);
    if (it_room_id == uid_to_roomid_.end()) return RoomRef{};
    const auto it = rooms_.find(it_room_id->second);
    return it != rooms_.end() ? it->second : RoomRef{};
}

core::actor::Future<bool> RoomManager::AddPlayerToRoom(const ROOM_ID_t room_id, AccountBaseData account_data, const UserConnectionPtr & userconn)
{
    const UID_t uid = account_data.uid();

    RoomRef ref;
    {
        std::shared_lock lock{mutex_};
        const auto it_room = rooms_.find(room_id);
        if (it_room != rooms_.end()) ref = it_room->second;
    }

    if (!ref) {
        auto promise = std::make_shared<core::actor::Promise<bool>>();
        auto future = promise->get_future();
        promise->set_value(false);
        return future;
    }

    //! 在 Room 线程内执行加入，成功后登记 uid 索引；actor 死亡时 Future 携带异常
    return ref.AskWith<bool>([this, account_data = std::move(account_data), userconn, uid, room_id](Room& room) mutable {
        const bool ok = room.AddPlayer(userconn, account_data);
        if (ok) {
            std::unique_lock lock{mutex_};
            AddUIDIndexLocked(uid, room_id);
        }
        return ok;
    });
}

}
