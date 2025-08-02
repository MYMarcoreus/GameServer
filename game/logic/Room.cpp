#include "Room.h"
#include "UserConnection.h"
#include "log.h"
#include "EventLoop.h"
#include "Timer.h"

using namespace yy::protocol::app;

namespace yy::app::logic
{
Room::Room(EventLoop * loop, const RoomDetailData& data):
    loop_(loop), room_data_(data)
{
}

Room::~Room()
{
    StopUpdate();
}

void Room::Init(const Milliseconds deltaTime)
{
    update_timer_id_ = loop_->RunEvery(deltaTime, [this] {
        this->Update();
    });

    // 自取消定时器
    const auto print_timer = loop_->CreateTimerEvery(1s);
    print_timer->SetCallback([weak_this = std::weak_ptr(shared_from_this()), timerid = print_timer->GetID(), loop = this->loop_] {
        if (const auto self = weak_this.lock()) {
            YLOG_INFO("房间 {}:{} 内有 ({} or {})/{} 人, ",
                self->room_data_.breif_data().name(),
                self->room_data_.breif_data().room_id(),
                self->room_data_.breif_data().size(),
                // or
                self->players_.size(),
                self->room_data_.breif_data().capacity());
        } else {
            loop->CancelTimer(timerid);
        }
    });
    loop_->AddTimer(print_timer);
}

void Room::Update()
{
    //
}

void Room::StopUpdate()
{
    loop_->CancelTimer(update_timer_id_);
}

void Room::InitPlayerData(const PlayerBaseDataPtr& data, const uint64_t uid)
{
    data->mutable_account_data()->set_uid(uid);
    data->set_hp_current(100);
    data->set_hp_max(100);

    PlayerMove* movement = data->mutable_movement();
    Transform_net* transform = movement->mutable_transform();
    Vector3_net* position = transform->mutable_position();
    position->set_x(0);
    position->set_y(0);
    position->set_z(0);

    Quaternion_net* rotation = transform->mutable_rotation();
    rotation->set_x(0);
    rotation->set_y(0);
    rotation->set_z(0);
    rotation->set_w(1);

    movement->set_ani_motion_speed(0);
    movement->set_ani_speed(0);
}

PlayerPtr Room::AddPlayer(const UserConnectionPtr& conn, const PlayerBaseDataPtr& data)
{
    YLOG_INFO("Room::AddPlayer {}", data->DebugString())
    //! room_data_
    YLOG_INFO("Room::room_data_ {}", room_data_.DebugString())
    room_data_.add_exist_player_datas()->CopyFrom(data->account_data());
    const auto brief_data = room_data_.mutable_breif_data();
    brief_data->set_size(brief_data->size()+1);
    YLOG_INFO("Room::room_data_ {}", room_data_.DebugString())

    //! players_
    auto [it, rst] = players_.emplace(data->account_data().uid(), std::make_shared<Player>(conn, data));
    return rst ? it->second : nullptr;
}

PlayerPtr Room::RemovePlayer(const UID_t uid)
{
    YLOG_INFO("Room::RemovePlayer {}", uid)
    //! room_data_
    auto* repeated = room_data_.mutable_exist_player_datas();
    for (int i = repeated->size() - 1; i >= 0; --i) {
        if (repeated->Get(i).uid() == uid) {
            repeated->DeleteSubrange(i, 1);
            break; // 如果只会存在一个，直接 break 更高效
        }
    }
    const auto brief_data = room_data_.mutable_breif_data();
    if (brief_data->size() > 0) {
        brief_data->set_size(brief_data->size()-1);
    }

    //! players_
    const auto it = players_.find(uid);
    PlayerPtr player = nullptr;
    if (it != players_.end()) {
        player = it->second;
        players_.erase(it);
    }
    return player;
}

PlayerPtr Room::FindPlayer(const UID_t uid)
{
    const auto it = players_.find(uid);
    return it == players_.end() ? nullptr : it->second;
}

std::unordered_map<UID_t, PlayerPtr> Room::GetAllPlayers()
{
    return players_;
}

bool Room::HasPlayer(const UID_t uid) const
{
    return players_.contains(uid);
}

void Room::Broadcast(const PlayerPtr& from, const google::protobuf::Message& data)
{
    for(const auto& to : players_ | std::views::values)
    {
        if(to->GetUID() == from->GetUID())
            continue;

        YLOG_TRACE("Broadcast<{}>: from {} to {}, ", data.GetDescriptor()->full_name(), from->GetUID(), to->GetUID())
        to->GetConn()->SendUDP(data);
    }
}

void Room::Broadcast(const PlayerPtr& from, const core::MessagePtr& data)
{
    if (from and data) {
        this->Broadcast(from, *data);
    }
}

void Room::Broadcast(const UID_t from_uid, const google::protobuf::Message& data)
{
    for(const auto& to : players_ | std::views::values)
    {
        if(to->GetUID() == from_uid)
            continue;

        YLOG_TRACE("Broadcast<{}>: from {} to {}, ", data.GetDescriptor()->full_name(), from_uid, to->GetUID())
        to->GetConn()->SendUDP(data);
    }
}
}
