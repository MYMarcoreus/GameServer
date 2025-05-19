#include "GamePlayerManager.h"
#include "GameManager.h"
#include "log.h"
#include <functional>

using namespace yy::core;
using namespace yy::util;
using yy::net::TcpConnectionPtr;

using yy::protocol::app::LoginRequest;
using yy::protocol::app::OtherPlayerDataRequest;
using yy::protocol::app::SelfMovement;
using yy::protocol::app::SelfJumpAndGravity;
using yy::protocol::app::PlayerLeave;

using yy::protocol::app::OtherMovement;
using yy::protocol::app::OtherJumpAndGravity;


using yy::protocol::app::OtherPlayerDataResponse;
using yy::protocol::app::PlayerBaseData;
using yy::protocol::app::PlayerMove;

namespace yy::app {



GamePlayerManager::GamePlayerManager()
    : m_global_id{10000}, m_server{GameManager::getInstance().GetServer()},
      m_player_pool{m_server->GetAppConfig().app_player_max()}
{
    GameManager::getInstance().RegisterMessageCallback<LoginRequest>(
            std::bind_front(&GamePlayerManager::OnLogin, this));
    GameManager::getInstance().RegisterMessageCallback<OtherPlayerDataRequest>(
            std::bind_front(&GamePlayerManager::OnOtherPlayerDataRequest, this));
    GameManager::getInstance().RegisterMessageCallback<SelfMovement>(
            std::bind_front(&GamePlayerManager::OnSelfMovement, this));
    GameManager::getInstance().RegisterMessageCallback<SelfJumpAndGravity>(
            std::bind_front(&GamePlayerManager::OnSelfJumpAndGravity, this));
    GameManager::getInstance().RegisterMessageCallback<PlayerLeave>(
            std::bind_front(&GamePlayerManager::OnLeave, this));

    // m_server->RunTaskEvery(1s, [this]() {
    //     std::lock_guard lg{this->m_online_players_mutex};
    //
    //     for(auto it = m_online_players.begin(); it != m_online_players.end() ;it++) {
    //         if()
    //     }
    // });
}

GamePlayerManager::~GamePlayerManager() // NOLINT(modernize-use-equals-default)
{
    // todo
}


void GamePlayerManager::Init()
{
    YLOG_TRACE("GamePlayerManager Init")
}

#if 0
void GamePlayerManager::Update()
{
    // YLOG_TRACE("GamePlayerManager StartListenAndIOLoop")
    static clock_t temptime = 0;
    auto value = clock() - temptime;
    if (value < 33) return;
    temptime = clock();

    INTERVAL_DO(1, YLOG_DEBUG("playernum = {}", m_online_players.size()) )
    for(auto it = m_online_players.begin() ; it != m_online_players.end() ;)
    {
        auto playerdata = it->second;
        auto userdata = m_server->FindUser(playerdata->conn_name());
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
            // m_server->DelUser(userdata);
            // playerdata->Clear();
            // m_player_pool.push(playerdata);
            //
            // it = m_online_players.erase(it); //!BUGFIXED
        }
        else {
            it++;
        }
    }
}
#endif

Ptr<yy::protocol::app::PlayerBaseData> GamePlayerManager::FindPlayerByUID(UID_t onlineid)
{
    std::lock_guard lg{m_online_players_mutex};

    auto it = m_online_players.find(onlineid);
    if(it==m_online_players.end()) {
        return nullptr;
    }
    else {
        return it->second;
    }
}

void GamePlayerManager::Broadcast(const UserConnectionPtr &from, const google::protobuf::Message &data) {
    decltype(m_online_players) players;
    {
        std::lock_guard lg{m_online_players_mutex};
        players = m_online_players;
    }

    for(const auto& p: players)
    {
        if(p.second->uid() == from->GetUID())
            continue;

        YLOG_TRACE("Broadcast<{}>: from {} to {}, ", data.GetDescriptor()->full_name(), from->GetUID(), p.second->uid())
        auto to = m_server->FindUser(p.second->conn_name());
        if(to == nullptr) continue;

        to->SendTCP(data);
    }
}

void GamePlayerManager::Broadcast(const UserConnectionPtr& from, const MessagePtr &data)
{
    if(from and data) {
        Broadcast(from, *data);
    }
}

void GamePlayerManager::LeaveAndSave(UserConnectionPtr leave_user) {
    // 给其他玩家客户端发送离线通告
    yy::protocol::app::PlayerLeave playerLeave;
    playerLeave.set_leaver_uid(leave_user->GetUID());
    Broadcast(leave_user, playerLeave);
    YLOG_INFO("玩家<{}>离开", playerLeave.leaver_uid())

    leave_user->SetState(core::UserConnection::E_UserBaseState::eSavingData);

    auto playerdata = FindPlayerByUID(leave_user->GetUID());
    // 重置数据，从在线玩家列表中删除，回收至对象池
    {
        std::lock_guard lg{m_online_players_mutex};
        m_online_players.erase(playerdata->uid());
    }
    playerdata->Clear();

    m_player_pool.push(playerdata);

    YLOG_INFO("玩家<{}>离开并保存数据！", leave_user->GetUID());

    leave_user->SetState(core::UserConnection::E_UserBaseState::eFree);
    m_server->DelUser(leave_user->GetConnName());
}












void GamePlayerManager::OnLogin(const UserConnectionPtr& userdata, const Ptr<protocol::app::LoginRequest> &) //NOLINT
{
    if(userdata->IsLoggedIn()) {
        return;
    }

    yy::protocol::app::LoginResponse loginResponse;

    // ①登陆请求：设置登录结果
    loginResponse.set_result(true);

    // ②登陆请求：初始化登录玩家对象，加入玩家数据列表
    userdata->SetUID(m_global_id++);
    auto selfdata = m_player_pool.pop();
    assert(selfdata != nullptr);

    // ③登陆请求：
    selfdata->set_uid(userdata->GetUID());
    selfdata->set_conn_name(userdata->GetConnName());
    selfdata->set_hp_current(100);
    selfdata->set_hp_max(100);

    // ④登陆请求：设置movement(初始position和rotation)
    {
        //! 若使用set_allocated，则需要主动传递堆空间，set_allocated会接管这片堆内存的管理权
        // PlayerMove  selfMove = new PlayerMove();
        // selfMove->set_position("");
        // selfMove->set_rotation("");
        // selfdata->set_allocated_player_move(selfMove);

        //! 使用mutable和*运算符进行赋值，让protobuf自己创建堆内存，算是一个小trick
        PlayerMove selfMove;
        selfMove.set_position("");
        selfMove.set_rotation("");
        *selfdata->mutable_movement() = selfMove;
    }

    // ⑤登陆请求：
    {
        decltype(m_online_players) players;
        {
            std::lock_guard lg{m_online_players_mutex};
            players = m_online_players;
        }

        // ②登陆请求：填充其他玩家数据
        for (const auto &p: players) {
            const auto &otherdata = *p.second;
            if(otherdata.uid() == selfdata->uid())
                continue;

            // add_othersdata为repeated字段增加元素，返回值就是该元素的指针
            auto data = loginResponse.add_other_datas();
            data->CopyFrom(otherdata); // 复制
        }

        {
            std::lock_guard lg{m_online_players_mutex};
            m_online_players.insert({selfdata->uid(), selfdata});
        }
    }

    *loginResponse.mutable_self_data() = *selfdata;


    YLOG_INFO("发送loginResponse = {}, {}, {}, {}, {}; size = {}",
              loginResponse.self_data().uid(),
              loginResponse.self_data().conn_name(),
              loginResponse.self_data().state(),
              loginResponse.self_data().hp_current(),
              loginResponse.self_data().hp_max(),
              loginResponse.ByteSizeLong()
    );


    // 转换状态
    // 返回给登录用户自己的信息和其他人的信息
    userdata->SendTCP(loginResponse);
    userdata->SetState(core::UserConnection::E_UserBaseState::eLoggedIn);

    // 返回登录用户的信息给其他用户
    OtherPlayerDataResponse otherPlayerDataResponse;
    *otherPlayerDataResponse.mutable_other_data() = *selfdata;
    YLOG_INFO("OtherPlayerDataResponse = {}, {}",
              otherPlayerDataResponse.other_data().uid(),
              otherPlayerDataResponse.other_data().conn_name());
    // Broadcast(userdata,  otherPlayerDataResponse);

    YLOG_INFO("玩家<{}:{}>登录", selfdata->conn_name(), selfdata->uid())
}

void GamePlayerManager::OnLeave(const UserConnectionPtr& userdata_self, const Ptr<protocol::app::PlayerLeave> & leave) //NOLINT
{
    if(userdata_self == nullptr) return;
    LeaveAndSave(userdata_self);
}

/// 当userdata_self收到其他人的移动的数据时，便会申请获取id为id_other的用户的玩家数据
void GamePlayerManager::OnOtherPlayerDataRequest(const UserConnectionPtr& userdata_self, const Ptr<protocol::app::OtherPlayerDataRequest> & request) //NOLINT
{
    auto player_other = FindPlayerByUID(request->requested_uid());
    if(player_other == nullptr) {
        YLOG_WARN("Get playerdata<{}> not found", request->requested_uid())
        return;
    }
    // auto userdata_other = m_server.FindUserBySockfd(player_other->sockfd);
    // return_if(userdata_other == nullptr);
    OtherPlayerDataResponse response;
    *response.mutable_other_data() = *player_other;
    userdata_self->SendTCP(response);
}

void GamePlayerManager::OnSelfMovement(const UserConnectionPtr& userdata_self, const Ptr<protocol::app::SelfMovement> & selfmove)
{
    auto playerdata_self = FindPlayerByUID(selfmove->uid());
    if(playerdata_self == nullptr) {
        YLOG_WARN("Get playerdata<{}> not found", selfmove->uid())
        return;
    }

    // playerdata_self->set_allocated_player_move(new PlayerMove(selfmove->self_move()));
    *playerdata_self->mutable_movement() = selfmove->movement();
    userdata_self->SendTCP(selfmove);

    OtherMovement othermove;
    othermove.set_uid(selfmove->uid());
    *othermove.mutable_movement() = selfmove->movement();

    Broadcast(userdata_self, othermove);
}

void GamePlayerManager::OnSelfJumpAndGravity(const UserConnectionPtr& userdata_self, const Ptr<protocol::app::SelfJumpAndGravity> & selfJumpAndGravity) //NOLINT
{
    auto playerdata_self = FindPlayerByUID(selfJumpAndGravity->uid());
    if(playerdata_self == nullptr) {
        YLOG_WARN("Jump playerdata<{}> not found", selfJumpAndGravity->uid())
        return;
    }

    // 记录玩家状态
    // playerdata_self->mutable_ani_jump_and_gravity()->CopyFrom(selfJumpAndGravity->self_jump_and_gravity());
    *playerdata_self->mutable_jump_and_gravity() = selfJumpAndGravity->jump_and_gravity();
    userdata_self->SendTCP(selfJumpAndGravity);

    OtherJumpAndGravity otherJumpAndGravity;
    otherJumpAndGravity.set_uid(selfJumpAndGravity->uid());
    *otherJumpAndGravity.mutable_jump_and_gravity() = selfJumpAndGravity->jump_and_gravity();

    Broadcast(userdata_self, otherJumpAndGravity);
}




} //namespace yy::app

