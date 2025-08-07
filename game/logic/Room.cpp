#include "Room.h"
#include "UserConnection.h"
#include "log.h"
#include "EventLoop.h"
#include "Timer.h"

using namespace yy::protocol::app;
using namespace yy::core;
using namespace yy::net;

namespace yy::app::logic
{
Room::Room(EventLoop * loop, const RoomDetailData& data): loop_(loop), room_data_(data),
    msg_handler_{[this](const UserConnectionPtr& userdata, const MessagePtr& message) {
        YLOG_DEBUG("未知的消息类型：{}", message->GetDescriptor()->full_name())
        userdata->Shutdown();
    }},
    players_pool_{util::ObjectPool<PlayerBaseData>::Instance()}
{
    RegisterHandler(this, this->msg_handler_  , &Room::OnEnterScene);
    RegisterHandler(this, this->msg_handler_  , &Room::OnLeaveScene);
    RegisterHandler(this, this->msg_handler_  , &Room::OnC2SOtherPlayerData);
    RegisterHandler(this, this->msg_handler_  , &Room::OnC2SMove);
    RegisterHandler(this, this->msg_handler_  , &Room::OnC2SJumpAndGravity);
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
            YLOG_INFO("房间 {}:{} 内有 ({})/{} 人, ",
                self->room_data_.name(),
                self->room_data_.room_id(),
                // or
                self->players_.size(),
                self->room_data_.capacity());
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

void Room::PostMessage(const UserConnectionPtr& conn, const MessagePtr& msg) const
{
    //! 即时处理（非Update）：对于游戏消息，并不在IO线程处理，而是在专门处理游戏数据的工作线程中处理（让分发器找到该游戏消息所注册的对应的处理函数。）
    loop_->RunCallbackInLoop([this, conn, msg] { // 注意这里跨线程传输需要拷贝智能指针
        // 调用消息对应的处理函数
        msg_handler_.OnProtobufMessage(conn, msg);
    });
}

void Room::PostTask(const F_TaskCallback& task) const
{
    loop_->RunCallbackInLoop(task);
}

void Room::OnPlayerDisconnect(const UserConnectionPtr& userconn)
{
    loop_->RunCallbackInLoop([this, userconn] {
        //! 给其他玩家客户端发送离线通告
         S2CLeaveScene playerLeave;
         playerLeave.set_uid(userconn->GetUID());
         Broadcast(userconn->GetUID(), playerLeave);
         YLOG_INFO("玩家<{}>被动离开", playerLeave.uid())

         userconn->SetState(UserConnection::E_UserBaseState::eSavingData);

         //! 清除数据 and 保存数据
         {
             // 删除玩家并将玩家数据归还对象池
             const auto removed_player = RemovePlayer(userconn->GetUID());
             removed_player->get_base_data()->Clear();
         }

         userconn->SetState(UserConnection::E_UserBaseState::eFree);

         //! 离开场景时，断开玩家与逻辑服的连接
         YLOG_INFO("玩家<{}>被动离开并保存数据！", userconn->GetUID());
    });
}

void Room::InitPlayerData(const PlayerBaseDataPtr& self_data, AccountBaseData account_data) //NOLINT
{
    self_data->mutable_account_data()->CopyFrom(std::move(account_data));
    self_data->set_hp_current(100);
    self_data->set_hp_max(100);

    PlayerMove* movement = self_data->mutable_movement();
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

void Room::AddPlayer(const UserConnectionPtr& self_conn, AccountBaseData account_data)
{
    loop_->RunCallbackInLoop([self_conn, account_data = std::move(account_data), this] {
        // 初始化进入玩家对象，加入玩家数据列表
        self_conn->SetUID(account_data.uid());
        const auto self_data = players_pool_.Acquire(2s);
        if(self_data == nullptr) {
            return;
        }
        InitPlayerData(self_data, account_data);

        //! room_data_
        room_data_.add_exist_player_datas()->CopyFrom(self_data->account_data());

        //! players_
        players_.emplace(self_data->account_data().uid(), std::make_shared<Player>(self_conn, self_data));
    });

}

PlayerPtr Room::RemovePlayer(const UID_t uid)
{
    YLOG_INFO("Room::RemovePlayer {}", uid)
    //! room_data_
    auto* repeated = room_data_.mutable_exist_player_datas();
    for (int i = repeated->size() - 1; i >= 0; --i) {
        if (repeated->Get(i).uid() == uid) {
            repeated->DeleteSubrange(i, 1);
            break;
        }
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

auto Room::GetAllPlayers() -> std::unordered_map<UID_t, PlayerPtr>
{
    return players_;
}

void Room::Broadcast(const PlayerPtr& from, const google::protobuf::Message& data)
{
    for(const auto& to : players_ | std::views::values)
    {
        if(to->get_uid() == from->get_uid())
            continue;

        YLOG_TRACE("Broadcast<{}>: from {} to {}, ", data.GetDescriptor()->full_name(), from->get_uid(), to->get_uid())
        to->get_conn()->SendUDP(data);
    }
}

void Room::Broadcast(const PlayerPtr& from, const MessagePtr& data)
{
    if (from and data) {
        this->Broadcast(from, *data);
    }
}

void Room::Broadcast(const UID_t from_uid, const google::protobuf::Message& data)
{
    for(const auto& to : players_ | std::views::values)
    {
        if(to->get_uid() == from_uid)
            continue;

        YLOG_TRACE("Broadcast<{}>: from {} to {}, ", data.GetDescriptor()->full_name(), from_uid, to->get_uid())
        to->get_conn()->SendUDP(data);
    }
}







void Room::OnEnterScene(const UserConnectionPtr& self_conn, const Ptr<protocol::app::C2SEnterScene> & req) //NOLINT
{
    if(self_conn == nullptr or req == nullptr) return;

    // 登录成功后会把玩家加入房间中，若找不到说明登录失败
    const PlayerPtr self_player = FindPlayer(self_conn->GetUID());
    if (self_player == nullptr or self_player->get_base_data() == nullptr) {
        YLOG_ERROR("{} 进入场景失败:！", req->uid());
        S2CEnterScene resp;
        resp.set_result(false);
        self_conn->SendTCP(resp);
        return;
    }
    const PlayerBaseDataPtr self_data = self_player->get_base_data();

    // 进入请求：填充其他玩家数据
    S2CEnterScene loginResponse;
    loginResponse.set_uid(req->uid());
    loginResponse.set_room_id(req->room_id());
    for (const PlayerPtr & other_player : GetAllPlayers() | std::views::values) {
        if(other_player->get_uid() == self_player->get_uid())
            continue;
        // add_othersdata为repeated字段增加元素，返回值就是该元素的指针
        PlayerBaseData* other_data = loginResponse.add_other_datas();
        other_data->CopyFrom(*other_player->get_base_data()); // 复制
    }
    loginResponse.set_result(true);
    loginResponse.mutable_self_data()->CopyFrom(*self_data);
    YLOG_INFO("发送loginResponse: {}", loginResponse.DebugString());
    // 返回给登录用户自己的信息和其他人的信息
    self_conn->SendTCP(loginResponse);

    // 返回登录用户的信息给其他用户
    S2COtherPlayerData otherPlayerDataResponse;
    otherPlayerDataResponse.set_uid(req->uid());
    otherPlayerDataResponse.set_room_id(req->room_id());
    otherPlayerDataResponse.mutable_other_data()->CopyFrom(*self_data);
    Broadcast(self_player,  otherPlayerDataResponse);

    YLOG_INFO("玩家<{}>进入场景", self_data->account_data().ShortDebugString())
}

void Room::OnLeaveScene(const UserConnectionPtr& leave_conn, const Ptr<protocol::app::C2SLeaveScene> & req) //NOLINT
{
    if(leave_conn == nullptr or req == nullptr) return;

    //! 给其他玩家客户端发送离线通告
    S2CLeaveScene playerLeave;
    playerLeave.set_uid(leave_conn->GetUID());
    Broadcast(req->uid(), playerLeave);
    YLOG_INFO("玩家<{}>主动离开", playerLeave.uid())

    leave_conn->SetState(UserConnection::E_UserBaseState::eSavingData);

    //! 清除数据 and 保存数据
    {
        // 删除玩家并将玩家数据归还对象池
        const auto removed_player = RemovePlayer(leave_conn->GetUID());
        removed_player->get_base_data()->Clear();
    }

    //! 离开场景时，断开玩家与逻辑服的连接
    leave_conn->Shutdown();
    YLOG_INFO("玩家<{}>主动离开并保存数据！", leave_conn->GetUID());
}

/// 当userdata_self收到其他人的移动的数据时，便会申请获取id为id_other的用户的玩家数据
void Room::OnC2SOtherPlayerData(const UserConnectionPtr& self_conn, const Ptr<C2SOtherPlayerData> & req)
{
    if(self_conn == nullptr or req == nullptr) return;

    const auto player_other = FindPlayer(req->requested_uid());
    if(player_other == nullptr) {
        YLOG_WARN("Get playerdata<{}> not found", req->requested_uid())
        return;
    }
    S2COtherPlayerData response;
    response.set_uid(req->requester_uid());
    response.set_room_id(req->room_id());
    response.mutable_other_data()->CopyFrom(*player_other->get_base_data());
    self_conn->SendTCP(response);
}

void Room::OnC2SMove(const UserConnectionPtr& self_conn, const Ptr<C2SMove> & selfmove)
{
    if(self_conn == nullptr or selfmove == nullptr) return;

    const auto player_self = FindPlayer(selfmove->uid());
    if(player_self == nullptr) {
        YLOG_WARN("Get playerdata<{}> not found", selfmove->uid())
        return;
    }

    player_self->get_base_data()->mutable_movement()->CopyFrom(selfmove->movement());
    self_conn->SendTCP(selfmove);

    S2CMove to_other_move;
    to_other_move.set_uid(selfmove->uid());
    to_other_move.set_room_id(selfmove->room_id());
    to_other_move.mutable_movement()->CopyFrom(selfmove->movement());

    Broadcast(player_self, to_other_move);
}

void Room::OnC2SJumpAndGravity(const UserConnectionPtr& self_conn, const Ptr<protocol::app::C2SJumpAndGravity> & selfJumpAndGravity) //NOLINT
{
    if(self_conn == nullptr or selfJumpAndGravity == nullptr) return;

    const auto player_self = FindPlayer(selfJumpAndGravity->uid());
    if(player_self == nullptr) {
        YLOG_WARN("Jump playerdata<{}> not found", selfJumpAndGravity->uid())
        return;
    }

    // 记录玩家状态
    player_self->get_base_data()->mutable_jump_and_gravity()->CopyFrom(selfJumpAndGravity->jump_and_gravity());
    self_conn->SendTCP(selfJumpAndGravity);

    S2CJumpAndGravity otherJumpAndGravity;
    otherJumpAndGravity.set_uid(selfJumpAndGravity->uid());
    otherJumpAndGravity.set_room_id(selfJumpAndGravity->room_id());
    otherJumpAndGravity.mutable_jump_and_gravity()->CopyFrom(selfJumpAndGravity->jump_and_gravity());

    Broadcast(player_self, otherJumpAndGravity);
}








}
