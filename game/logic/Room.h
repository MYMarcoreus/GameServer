#pragma once
#include "net_definations.h"
#include "Player.h"
#include "room_data.pb.h"
#include "Timestamp.h"

namespace yy::net { class EventLoop; }
using namespace yy::net;


namespace yy::app::logic
{

using ROOM_ID_t = uint64_t;
using namespace yy::protocol::app;

// 管理一个房间内的玩家：一个房间在一个线程中处理，线程安全，无锁
class Room: public std::enable_shared_from_this<Room>{
public:
    Room(EventLoop * loop, const RoomDetailData&);
    ~Room();

    EventLoop * GetLoop() const { return loop_; }

    void Init(Milliseconds deltaTime);

    void Update();
    void StopUpdate();

    void InitPlayerData(const PlayerBaseDataPtr& data, uint64_t uid);

    PlayerPtr AddPlayer(const UserConnectionPtr& conn, const PlayerBaseDataPtr& data);
    [[nodiscard]] auto RemovePlayer(UID_t uid) -> PlayerPtr;
    [[nodiscard]] auto FindPlayer(UID_t uid) -> PlayerPtr;
    [[nodiscard]] auto GetAllPlayers() -> std::unordered_map<UID_t, PlayerPtr>;
    [[nodiscard]] auto HasPlayer(UID_t uid) const -> bool;
    void Broadcast(const PlayerPtr& from, const google::protobuf::Message &data);
    void Broadcast(const PlayerPtr& from, const core::MessagePtr &data);
    void Broadcast(UID_t from_uid, const google::protobuf::Message &data);

    [[nodiscard]] RoomDetailData        get_room_data() const { return room_data_; }
    [[nodiscard]] UID_t                 get_owner_uid() const { return room_data_.breif_data().owner_uid(); }
    [[nodiscard]] const std::string&    get_name() const { return room_data_.breif_data().name(); }
    [[nodiscard]] ROOM_ID_t             get_id() const { return room_data_.breif_data().room_id(); }
    [[nodiscard]] int                   get_capacity() const { return room_data_.breif_data().capacity(); }

private:
    EventLoop *                             loop_;
    TimerID                                 update_timer_id_;
    RoomDetailData                          room_data_;
    std::unordered_map<UID_t, PlayerPtr>    players_;
};

using RoomPtr = std::shared_ptr<Room>;

}
