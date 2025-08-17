#include "RoomManager.h"
#include "AppXmlConfig.h"
#include "EventLoop.h"
#include "EventLoopThread.h"
#include "EventLoopThreadPool.h"
#include <ranges>

using namespace yy::net;
using namespace yy::core;
using namespace yy::app;
using namespace yy::protocol::app;

namespace yy::app::logic
{
RoomManager::RoomManager(EventLoop * base_loop): base_loop_(base_loop)
{
    work_threads = std::make_unique<EventLoopThreadPool>(base_loop_);
    work_threads->Start(config::g_app_config->GetValue().work_thread_num(), 500ms); //! 启动服务器的工作线程：即时处理

    base_loop_->RunEvery(1s, [this]() {
        for (auto [room_id, room]: this->rooms_) {
            YLOG_INFO("房间{}: {}", room_id, room->get_room_data().ShortDebugString())
        }
        for (auto [uid, room_id]: this->uid_to_roomid_) {
            YLOG_INFO("uid:{} - rooid:{}", uid, room_id)
        }
    });
}

void RoomManager::AddRoom(RoomDetailData room_data, std::function<void(RoomPtr)> done)
{
    assert(done);
    if (!done) return;

    base_loop_->RunCallbackInLoop([this, room_data = std::move(room_data), done = std::move(done)] {
        // 初始化房间对象，分配房间对应的线程，开启Update
        auto room = std::make_shared<Room>(work_threads->GetNextLoop(), room_data);
        room->Init(ROOM_TICK);
        room->SetPlayerRemoveCallback([this](UID_t uid) {
            base_loop_->RunCallbackInLoop([this, uid] {
                uid_to_roomid_.erase(uid);
            });
        });

        // 加入房间列表
        rooms_.emplace(room->get_id(), room);
        // uid_to_roomid_.emplace(room->get_owner_uid(), room->get_id());
        uid_to_roomid_[room->get_owner_uid()] = room->get_id();
        return done(room);
    });
}

void RoomManager::RemoveRoom(const ROOM_ID_t room_id, std::function<void(bool)> done)
{
    assert(done);
    if (!done) return;

    base_loop_->RunCallbackInLoop([this, room_id, done = std::move(done)] {
        const auto it = rooms_.find(room_id);
        const auto room = (it != rooms_.end() ? it->second : nullptr);
        if (room == nullptr) {
            return done(false);
        }
        for (const auto& player_uid : room->GetAllPlayers() | std::views::keys) {
            uid_to_roomid_.erase(player_uid);
        }
        return done(rooms_.erase(room_id) > 0);
    });
}

void RoomManager::FindRoomByRoomID(const ROOM_ID_t room_id, std::function<void(RoomPtr)> done)
{
    assert(done);
    if (!done) return;

    base_loop_->RunCallbackInLoop([this, room_id, done = std::move(done)] {
        const auto it = rooms_.find(room_id);
        done(it != rooms_.end() ? it->second : nullptr);
    });
}

void RoomManager::FindRoomByUID(const UID_t uid, std::function<void(RoomPtr)> done)
{
    assert(done);
    if (!done) return;

    base_loop_->RunCallbackInLoop([this, uid, done = std::move(done)] {
        const auto it_room_id = uid_to_roomid_.find(uid);
        if (it_room_id == uid_to_roomid_.end()) {
            return done(nullptr);
        }
        const auto it = rooms_.find(it_room_id->second);
        done(it != rooms_.end() ? it->second : nullptr);
    });
}

void RoomManager::FindRoomIDByUID(const UID_t uid, std::function<void(std::optional<ROOM_ID_t>)> done)
{
    assert(done);
    if (!done) return;

    base_loop_->RunCallbackInLoop([this, uid, done = std::move(done)] {
        const auto it_room_id = uid_to_roomid_.find(uid);
        if (it_room_id == uid_to_roomid_.end()) {
            return done(std::nullopt);
        }
        return done(it_room_id->second);
    });
}

void RoomManager::AddPlayerToRoom(ROOM_ID_t room_id, AccountBaseData account_data, const UserConnectionPtr & userconn, std::function<void(RoomPtr)> done)
{
    assert(done);
    if (!done) return;

    base_loop_->RunCallbackInLoop([this, room_id, account_data = std::move(account_data), done = std::move(done), userconn] {
        // 查找要加入的房间
        const auto it_room = rooms_.find(room_id);
        const auto room = (it_room != rooms_.end() ? it_room->second : nullptr);
        if (room == nullptr) {
            return done(nullptr);
        }
        room->AddPlayer(userconn, account_data);
        uid_to_roomid_[account_data.uid()] = room->get_id();
        return done(room);
    });
}

}
