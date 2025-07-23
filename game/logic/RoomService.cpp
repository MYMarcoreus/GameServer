#include <functional>
#include "RoomService.h"
#include "LogicServerManager.h"
#include "log.h"
#include "UserConnection.h"
#include "IServer.h"

using namespace yy::core;
using namespace yy::util;
using namespace yy::protocol::app;

namespace yy::app::logic {
RoomService::RoomService():
    server_{LogicServerManager::Instance().GetServer()},
    players_pool_{ObjectPool<PlayerBaseData>::Instance()}
{
    players_pool_.Init("玩家对象池", server_.GetAppConfig().app_player_max(), nullptr, nullptr);
    LogicServerManager::Instance().RegisterHandler(this, &RoomService::OnEnterRoom);
    LogicServerManager::Instance().RegisterHandler(this, &RoomService::OnLeaveRoom);
    LogicServerManager::Instance().RegisterHandler(this, &RoomService::OnC2SOtherPlayerData);
    LogicServerManager::Instance().RegisterHandler(this, &RoomService::OnC2SMove);
    LogicServerManager::Instance().RegisterHandler(this, &RoomService::OnC2SJumpAndGravity);
}

RoomService::~RoomService() = default;


void RoomService::Init()
{
    YLOG_TRACE("RoomService Init")
}

#if 0
void RoomService::Update()
{
    // YLOG_TRACE("RoomService StartListenAndIOLoop")
    static clock_t temptime = 0;
    auto value = clock() - temptime;
    if (value < 33) return;
    temptime = clock();

    INTERVAL_DO(1, YLOG_DEBUG("playernum = {}", m_room_players.size()) )
    for(auto it = m_room_players.begin() ; it != m_room_players.end() ;)
    {
        auto playerdata = it->second;
        auto userdata = server_.FindUser(playerdata->conn_name());
        if(userdata == nullptr){
            it++;
            continue;
        }

        INTERVAL_DO(1, YLOG_DEBUG("player<{},{}>", playerdata->conn_name(), playerdata->uid()) )

        // Check_Offline
        if(userdata->isNeedSave())
        {
            // // 给其他玩家客户端发送离线通告
            // yy::protocol::app::PlayerLeave playerLeave;
            // playerLeave.set_uid(playerdata->uid());
            // Broadcast(userdata, playerLeave);
            // YLOG_INFO("玩家<{}>离开，数据已保存", playerLeave.uid())
            //
            // // 重置数据，从在线玩家列表中删除，回收至对象池
            // server_.DelUser(userdata);
            // playerdata->Clear();
            // players_pool_.push(playerdata);
            //
            // it = m_room_players.erase(it); //!BUGFIXED
        }
        else {
            it++;
        }
    }
}
#endif

PlayerPtr RoomService::FindPlayerByUID(const UID_t uid)
{
    return players_.GetPlayer(uid);
}

void RoomService::InitPlayerData(const PlayerBaseDataPtr & data, const uint64_t uid)
{
    data->set_uid(uid);
    data->set_hp_current(100);
    data->set_hp_max(100);
    PlayerMove move;
    move.set_position("");  // todo 可进一步替换为默认初始坐标
    move.set_rotation("");
    data->mutable_movement()->CopyFrom(move);
}

PlayerPtr RoomService::CreatePlayer(const UserConnectionPtr& conn, const PlayerBaseDataPtr& data)
{
    return std::make_shared<Player>(conn, data);
}

void RoomService::LeaveAndSave(const UserConnectionPtr& leave_conn) {
    // 给其他玩家客户端发送离线通告
    C2SLeaveRoom playerLeave;
    playerLeave.set_leaver_uid(leave_conn->GetUID());
    players_.Broadcast(FindPlayerByUID(leave_conn->GetUID()), playerLeave);
    YLOG_INFO("玩家<{}>离开", playerLeave.leaver_uid())

    leave_conn->SetState(UserConnection::E_UserBaseState::eSavingData);
    // 删除玩家并将玩家数据归还对象池
    auto removed_player = players_.RemovePlayer(leave_conn->GetUID());
    removed_player->GetBaseData()->Clear();
    removed_player.reset();

    leave_conn->SetState(UserConnection::E_UserBaseState::eFree);
    server_.DelUser(leave_conn->GetConnID());
    YLOG_INFO("玩家<{}>离开并保存数据！", leave_conn->GetUID());
}












void RoomService::OnEnterRoom(const UserConnectionPtr& self_conn, const Ptr<protocol::app::C2SEnterRoom> & req) //NOLINT
{
    auto SendEnterSceneFailed = [&](std::string_view reason) {
        YLOG_ERROR("进入场景失败: {}", reason);
        S2CEnterRoom resp;
        resp.set_result(false);
        self_conn->SendTCP(resp);
    };

    // 防止同个连接重复进入场景
    if(self_conn->IsLoggedIn()) {
        SendEnterSceneFailed("当前连接已在场景中，无需重复进入");
        return;
    }

    // 初始化进入玩家对象，加入玩家数据列表
    self_conn->SetUID(req->uid());
    const auto self_data = players_pool_.Acquire(2s);
    if(self_data == nullptr) {
        SendEnterSceneFailed("玩家对象池资源不足，不准进入场景！");
        return;
    }
    InitPlayerData(self_data, req->uid());
    const PlayerPtr self_player = CreatePlayer(self_conn, self_data);
    players_.AddPlayer(self_data->uid(), self_player);

    // 进入请求：填充其他玩家数据
    S2CEnterRoom loginResponse;
    for (const PlayerPtr & other_player : players_.GetAllPlayers() | std::views::values) {
        if(other_player->GetUID() == self_player->GetUID())
            continue;
        // add_othersdata为repeated字段增加元素，返回值就是该元素的指针
        PlayerBaseData* other_data = loginResponse.add_other_datas();
        other_data->CopyFrom(*other_player->GetBaseData()); // 复制
    }
    loginResponse.set_result(true);
    loginResponse.mutable_self_data()->CopyFrom(*self_data);

    YLOG_INFO("发送loginResponse: {}", loginResponse.DebugString());

    // 转换状态
    // 返回给登录用户自己的信息和其他人的信息
    self_conn->SendTCP(loginResponse);
    self_conn->SetState(UserConnection::E_UserBaseState::eLoggedIn);

    // 返回登录用户的信息给其他用户
    S2COtherPlayerData otherPlayerDataResponse;
    otherPlayerDataResponse.mutable_other_data()->CopyFrom(*self_data);
    players_.Broadcast(self_player,  otherPlayerDataResponse);

    YLOG_INFO("玩家<{}>进入场景", self_data->uid())
}

void RoomService::OnLeaveRoom(const UserConnectionPtr& self_conn, const Ptr<protocol::app::C2SLeaveRoom> & leave) //NOLINT
{
    if(self_conn == nullptr) return;
    LeaveAndSave(self_conn);
}

/// 当userdata_self收到其他人的移动的数据时，便会申请获取id为id_other的用户的玩家数据
void RoomService::OnC2SOtherPlayerData(const UserConnectionPtr& self_conn, const Ptr<protocol::app::C2SOtherPlayerData> & request) //NOLINT
{
    const auto player_other = FindPlayerByUID(request->requested_uid());
    if(player_other == nullptr) {
        YLOG_WARN("Get playerdata<{}> not found", request->requested_uid())
        return;
    }
    S2COtherPlayerData response;
    response.mutable_other_data()->CopyFrom(*player_other->GetBaseData());
    self_conn->SendTCP(response);
}

void RoomService::OnC2SMove(const UserConnectionPtr& self_conn, const Ptr<C2SMove> & selfmove)
{
    const auto player_self = FindPlayerByUID(selfmove->uid());
    if(player_self == nullptr) {
        YLOG_WARN("Get playerdata<{}> not found", selfmove->uid())
        return;
    }

    player_self->GetBaseData()->mutable_movement()->CopyFrom(selfmove->movement());
    self_conn->SendTCP(selfmove);

    S2CMove to_other_move;
    to_other_move.set_uid(selfmove->uid());
    to_other_move.mutable_movement()->CopyFrom(selfmove->movement());

    players_.Broadcast(player_self, to_other_move);
}

void RoomService::OnC2SJumpAndGravity(const UserConnectionPtr& self_conn, const Ptr<protocol::app::C2SJumpAndGravity> & selfJumpAndGravity) //NOLINT
{
    const auto player_self = FindPlayerByUID(selfJumpAndGravity->uid());
    if(player_self == nullptr) {
        YLOG_WARN("Jump playerdata<{}> not found", selfJumpAndGravity->uid())
        return;
    }

    // 记录玩家状态
    player_self->GetBaseData()->mutable_jump_and_gravity()->CopyFrom(selfJumpAndGravity->jump_and_gravity());
    self_conn->SendTCP(selfJumpAndGravity);

    S2CJumpAndGravity otherJumpAndGravity;
    otherJumpAndGravity.set_uid(selfJumpAndGravity->uid());
    *otherJumpAndGravity.mutable_jump_and_gravity() = selfJumpAndGravity->jump_and_gravity();

    players_.Broadcast(player_self, otherJumpAndGravity);
}




} //namespace yy::app

