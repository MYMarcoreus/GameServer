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
            std::bind(&GamePlayerManager::onLogin, this, _1, _2));
    GameManager::getInstance().RegisterMessageCallback<OtherPlayerDataRequest>(
            std::bind( &GamePlayerManager::onOtherPlayerDataRequest, this, _1, _2));
    GameManager::getInstance().RegisterMessageCallback<SelfMovement>(
            std::bind(&GamePlayerManager::onSelfMovement, this, _1, _2));
    GameManager::getInstance().RegisterMessageCallback<SelfJumpAndGravity>(
            std::bind(&GamePlayerManager::onSelfJumpAndGravity, this, _1, _2));
    GameManager::getInstance().RegisterMessageCallback<OtherMovement>(
            std::bind(&GamePlayerManager::onOtherMovement, this, _1, _2));
    GameManager::getInstance().RegisterMessageCallback<OtherJumpAndGravity>(
            std::bind(&GamePlayerManager::onOtherJumpAndGravity, this, _1, _2));
    GameManager::getInstance().RegisterMessageCallback<PlayerLeave>(
            std::bind(&GamePlayerManager::onLeave, this, _1, _2));
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
    // YLOG_TRACE("GamePlayerManager Update")
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
            // m_server->SetUserFree(userdata);
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


void GamePlayerManager::Broadcast(const UserBaseDataPtr &from, const google::protobuf::Message &data) {
    std::lock_guard lg{m_online_players_mutex};

    for(const auto& p: m_online_players)
    {
        YLOG_INFO("Broadcast: {},{}", p.second->uid(), from->GetUID())

        if(p.second->uid() == from->GetUID())
            continue;
        auto to = m_server->FindUser(p.second->conn_name());
        if(to == nullptr) continue;

        from->Send(data);
    }
}

void GamePlayerManager::Broadcast(const UserBaseDataPtr& from, const MessagePtr &data)
{
    if(data) {
        Broadcast(from, *data);
    }
}














void GamePlayerManager::onLogin(const UserBaseDataPtr& userdata, const Ptr<LoginRequest> & loginRequest ) //NOLINT
{
    if(userdata->isLoggedIn()) {
        return;
    }

    yy::protocol::app::LoginResponse loginResponse;

    // ①登陆请求：设置登录结果
    loginResponse.set_result(true);


    // ③登陆请求：初始化登录玩家对象，加入玩家数据列表
    userdata->SetUID(m_global_id++);
    auto selfdata = m_player_pool.pop();
    assert(selfdata != nullptr);
    selfdata->set_uid(userdata->GetUID());
    selfdata->set_conn_name(userdata->GetConnection()->GetName());
    selfdata->set_hp_current(100);
    selfdata->set_hp_max(100);

    //! 若使用set_allocated，则需要主动传递堆空间，set_allocated会接管这片堆内存的管理权
    // PlayerMove  selfMove = new PlayerMove();
    // selfMove->set_position("");
    // selfMove->set_rotation("");
    // selfdata->set_allocated_player_move(selfMove);

    //! 使用mutable和*运算符进行赋值，让protobuf自己创建堆内存，算是一个小trick
    PlayerMove selfMove;
    selfMove.set_position("");
    selfMove.set_rotation("");
    *selfdata->mutable_player_move() = selfMove;

    {
        std::lock_guard lg{m_online_players_mutex};

        // ②登陆请求：填充其他玩家数据
        for (const auto &p: m_online_players) {
            const auto &otherdata = *p.second;
            // add_othersdata为repeated字段增加元素，返回值就是该元素的指针
            auto data = loginResponse.add_other_datas();
            data->CopyFrom(otherdata); // 复制
        }

        m_online_players.insert({selfdata->uid(), selfdata});
    }

    *loginResponse.mutable_self_data() = *selfdata;



    {
        YLOG_INFO("loginResponse = {}, {}, {}, {}, {}; size = {}",
                  loginResponse.self_data().uid(),
                  loginResponse.self_data().conn_name(),
                  loginResponse.self_data().state(),
                  loginResponse.self_data().hp_current(),
                  loginResponse.self_data().hp_max(),
                  loginResponse.ByteSizeLong()
        );
    }


    // 转换状态
    // 返回给登录用户自己的信息和其他人的信息
    userdata->Send(loginResponse);
    userdata->SetState(core::UserBaseData::E_UserBaseState::eLoggedIn);

    // 返回登录用户的信息给其他用户
    OtherPlayerDataResponse otherPlayerDataResponse;
    *otherPlayerDataResponse.mutable_other_data() = *selfdata;
    Broadcast(userdata,  otherPlayerDataResponse);

    YLOG_INFO("玩家<{}:{}>登录", selfdata->conn_name(), selfdata->uid())
}

void GamePlayerManager::onLeave(const UserBaseDataPtr& userdata_self, const Ptr<protocol::app::PlayerLeave> & leave) //NOLINT
{
    if(userdata_self == nullptr) return;

    auto playerdata = FindPlayerByUID(userdata_self->GetUID());

    // 给其他玩家客户端发送离线通告
    yy::protocol::app::PlayerLeave playerLeave;
    playerLeave.set_uid(playerdata->uid());
    Broadcast(userdata_self, playerLeave);
    YLOG_INFO("玩家<{}>离开，数据已保存", playerLeave.uid())

    // 重置数据，从在线玩家列表中删除，回收至对象池
    m_server->SetUserFree(userdata_self);
    playerdata->Clear();
    m_player_pool.push(playerdata);

    std::lock_guard lg{m_online_players_mutex};
    m_online_players.erase(playerdata->uid());
}

void GamePlayerManager::onOtherMovement(const UserBaseDataPtr& userdata_self, const Ptr<protocol::app::OtherMovement> & othermove)
{
    // 收到玩家A移动后的数据
    auto playerdata_self = FindPlayerByUID(othermove->uid());
    if(playerdata_self == nullptr) {
        YLOG_WARN("Get playerdata<{}> not found", othermove->uid())
        return;
    }

    // playerdata_self->set_allocated_player_move(new PlayerMove(othermove->other_move()));
    *playerdata_self->mutable_player_move() = othermove->other_move();

    // 转发给其它玩家
    Broadcast(userdata_self, othermove);

//    INTERVAL_DO(1, YLOG_DEBUG("玩家<%d>移动：%s, %s", playerdata_self->uid(),
//                   othermove.position.ToString().c_str(), othermove.rotation.ToString().c_str()) )
}

void GamePlayerManager::onSelfMovement(const UserBaseDataPtr& userdata_self, const Ptr<protocol::app::SelfMovement> & selfmove)
{
    auto playerdata_self = FindPlayerByUID(selfmove->uid());
    if(playerdata_self == nullptr) {
        YLOG_WARN("Get playerdata<{}> not found", selfmove->uid())
        return;
    }

    // playerdata_self->set_allocated_player_move(new PlayerMove(selfmove->self_move()));
    *playerdata_self->mutable_player_move() = selfmove->self_move();


    userdata_self->Send(selfmove);
}


/// 当userdata_self收到其他人的移动的数据时，便会申请获取id为id_other的用户的玩家数据
void GamePlayerManager::onOtherPlayerDataRequest(const UserBaseDataPtr& userdata_self, const Ptr<protocol::app::OtherPlayerDataRequest> & request) //NOLINT
{

    auto player_other = FindPlayerByUID(request->uid());
    if(player_other == nullptr) {
        YLOG_WARN("Get playerdata<{}> not found", request->uid())
        return;
    }
    // auto userdata_other = m_server.FindUserBySockfd(player_other->sockfd);
    // return_if(userdata_other == nullptr);
    OtherPlayerDataResponse response;
    *response.mutable_other_data() = *player_other;
    userdata_self->Send(response);
}

void GamePlayerManager::onOtherJumpAndGravity(const UserBaseDataPtr& userdata_self, const Ptr<protocol::app::OtherJumpAndGravity> & otherJumpAndGravity) //NOLINT
{
    auto playerdata_self = FindPlayerByUID(otherJumpAndGravity->uid());
    if(playerdata_self == nullptr) {
        YLOG_WARN("Jump playerdata<{}> not found", otherJumpAndGravity->uid())
        return;
    }

    // 记录玩家状态
    *playerdata_self->mutable_ani_jump_and_gravity() = otherJumpAndGravity->other_jump_and_gravity();
    Broadcast(userdata_self, otherJumpAndGravity);
}


void GamePlayerManager::onSelfJumpAndGravity(const UserBaseDataPtr& userdata_self, const Ptr<protocol::app::SelfJumpAndGravity> & selfJumpAndGravity) //NOLINT
{
    auto playerdata_self = FindPlayerByUID(selfJumpAndGravity->uid());
    if(playerdata_self == nullptr) {
        YLOG_WARN("Jump playerdata<{}> not found", selfJumpAndGravity->uid())
        return;
    }

    // 记录玩家状态
    // playerdata_self->mutable_ani_jump_and_gravity()->CopyFrom(selfJumpAndGravity->self_jump_and_gravity());
    *playerdata_self->mutable_ani_jump_and_gravity() = selfJumpAndGravity->self_jump_and_gravity();
    Broadcast(userdata_self, selfJumpAndGravity);
}



} //namespace yy::app
