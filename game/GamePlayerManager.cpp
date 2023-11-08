#include "GamePlayerManager.h"
#include "log.h"

using namespace yy::core;
using namespace yy::util;
using yy::net::TcpConnectionPtr;

namespace yy::app {



GamePlayerManager::GamePlayerManager()
    : m_global_id{10000}, m_server{get_server_instance()},
      m_player_pool{m_server.GetAppConfig().app_player_max()}
{ }

GamePlayerManager::~GamePlayerManager() // NOLINT(modernize-use-equals-default)
{
    // todo
}


void GamePlayerManager::Init()
{
    YLOG_TRACE("GamePlayerManager Init")
}

void GamePlayerManager::Update()
{
    // YLOG_TRACE("GamePlayerManager Update")
    static clock_t temptime = 0;
    auto value = clock() - temptime;
    if (value < 33) return;
    temptime = clock();

    INTERVAL_DO(1, YLOG_DEBUG("playernum = %zu", m_online_players.size()) )
    for(auto it = m_online_players.begin() ; it != m_online_players.end() ;)
    {
        auto playerdata = it->second;
        auto userdata = m_server.FindUserBySockfd(playerdata->sockfd());
        if(userdata == nullptr){
            it++;
            continue;
        }

        INTERVAL_DO(1, YLOG_DEBUG("player{%d,%d}", playerdata->sockfd(), playerdata->uid()) )

        // Check_Offline
        if(userdata->isNeedSave())
        {
            // 给其他玩家客户端发送离线通告
            protocol::PlayerID playerId;
            playerId.set_uid(playerdata->uid());
            Broadcast(playerdata->uid(), E_PackageCommand::eLeave, playerId);
            YLOG_INFO("玩家<%d>离开，数据已保存", playerdata->uid())

            // 重置数据，从在线玩家列表中删除，回收至对象池
            m_server.setUserFree(userdata);
            playerdata->Clear();
            m_player_pool.push(playerdata);

            it = m_online_players.erase(it); //!BUGFIXED
        }
        else {
            it++;
        }
    }
}

Ptr<protocol::PlayerBaseData> GamePlayerManager::FindPlayerByUID(UID_t onlineid)
{
    auto it = m_online_players.find(onlineid);
    if(it==m_online_players.end()) {
        return nullptr;
    }
    else {
        return it->second;
    }
}

void GamePlayerManager::AppCommand(const TcpConnectionPtr & conn, const google::protobuf::Message & message)
{
    auto & userdata = m_server.FindUser(conn);


}

void GamePlayerManager::onLogin(const UserBaseDataPtr& userdata) //NOLINT
{
    return_if(userdata->isLoggedIn());

    yy::app::protocol::LoginResponse loginResponse;

    // ①登陆请求：设置登录结果
    loginResponse.set_result(true);

    // ②登陆请求：获取其他玩家数据，并序列化
    for(const auto& p: m_online_players) {
        const auto & otherdata = *p.second;
        // add_othersdata为repeated字段增加元素，返回值就是该元素的指针
        auto data = loginResponse.add_othersdata();
        data->CopyFrom(otherdata); // 复制
    }

    // ③登陆请求：初始化登录玩家对象，加入玩家数据列表
    auto selfdata = m_player_pool.pop();
    assert(selfdata != nullptr);
    selfdata->set_uid(m_global_id++);
    selfdata->set_sockfd(userdata->sock.get_fd());
    selfdata->set_hp_current(100);
    selfdata->set_hp_max(100);
    m_online_players.insert({selfdata->uid(), selfdata});
    *loginResponse.mutable_selfdata() = *selfdata;

    // 转换状态
    userdata->state = E_ServerSocketState::eLoggedIn;

    // 返回给登录用户自己的信息和其他人的信息
    m_server.BuildPackage(userdata, E_PackageCommand::eLogin, &loginResponse);

    // 返回登录用户的信息给其他用户
    Broadcast(selfdata->uid(), E_PackageCommand::eGetPlayerData, *selfdata);

    YLOG_INFO("玩家<%d:%d>登录", selfdata->sockfd(), selfdata->uid())

}

void GamePlayerManager::onMove(const UserBaseDataPtr& userdata_self)
{
    // 收到玩家A移动后的数据
    protocol::PlayerMove playerMove;
    m_server.ParsePackage(userdata_self, &playerMove);

    auto playerdata_self = FindPlayerByUID(playerMove.uid());
    if(playerdata_self == nullptr) {
        YLOG_WARN("Get playerdata<%d> not found", playerMove.uid())
        return;
    }

    // 记录玩家位置和状态
    playerdata_self->set_position(playerMove.position());
    playerdata_self->set_rotation(playerMove.rotation());
    playerdata_self->set_ani_speed(playerMove.ani_speed());
    playerdata_self->set_ani_motionspeed(playerMove.ani_motionspeed());

    // 转发给其它玩家
    Broadcast(playerMove.uid(), E_PackageCommand::eMove ,playerMove);

//    INTERVAL_DO(1, YLOG_DEBUG("玩家<%d>移动：%s, %s", playerdata_self->uid(),
//                   playerMove.position.ToString().c_str(), playerMove.rotation.ToString().c_str()) )
}

/// 当userdata_self收到其他人的移动的数据时，便会申请获取id为id_other的用户的玩家数据
void GamePlayerManager::onGetPlayerData(const UserBaseDataPtr& userdata_self) //NOLINT
{
    protocol::PlayerID id_other;
    m_server.ParsePackage(userdata_self, &id_other);

    auto player_other = FindPlayerByUID(id_other.uid());
    if(player_other == nullptr) {
        YLOG_WARN("Get playerdata<%d> not found", id_other.uid())
        return;
    }
    // auto userdata_other = m_server.FindUserBySockfd(player_other->sockfd);
    // return_if(userdata_other == nullptr);

    m_server.BuildPackage(userdata_self, player_other.get());
}

void GamePlayerManager::onJumpAndGravity(const UserBaseDataPtr& userdata_self) //NOLINT
{
    protocol::PlayerJumpAndGravity playerJump;
    m_server.ParsePackage(userdata_self, &playerJump);

    auto playerdata_self = FindPlayerByUID(playerJump.uid());
    if(playerdata_self == nullptr) {
        YLOG_WARN("Jump playerdata<%d> not found", playerJump.uid())
        return;
    }

    // 记录玩家状态
    playerdata_self->set_ani_isjump    (playerJump.ani_isjump());
    playerdata_self->set_ani_isground  (playerJump.ani_isground());
    playerdata_self->set_ani_isfreefall(playerJump.ani_isfreefall());

    Broadcast(playerJump.uid(), playerJump);
}

void GamePlayerManager::onLeave(const UserBaseDataPtr& userdata_self) //NOLINT
{
    return_if(userdata_self == nullptr);
}


void GamePlayerManager::Broadcast(UID_t from, const google::protobuf::Message &data)
{
    for(const auto& p: m_online_players)
    {
        if(p.second->uid() == from)
            continue;
        auto to = m_server.FindUser(p.second->sockfd());
        continue_if(to == nullptr);

        m_server.BuildPackage(to, cmd, &data);
    }
}


} //namespace yy::app
