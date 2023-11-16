#ifndef ____GAMEPLAYERMANAGER_H
#define ____GAMEPLAYERMANAGER_H

#include "IGameBase.h"
#include "Singleton.h"
#include "GameData.h"
#include "ThreadSafeQueue.hpp"
#include "IServer.h"
#include "ObjectPool.h"
#include "core_definations.h"
#include <unordered_map>

using yy::core::UserBaseDataPtr;

namespace yy::app {

class GamePlayerManager final: public IGameBase, public Singleton<GamePlayerManager>
{
    SINGLETON_NECESSITY(GamePlayerManager)
public:
    void Init() override;

    // void Update() override;

private:
    GamePlayerManager();
    ~GamePlayerManager() override;

    /// @brief 玩家发来登录请求，验证，然后将储存的游戏数据发送回玩家
    void onLogin(const UserBaseDataPtr& userdata, const Ptr<protocol::app::LoginRequest> &);

    /// @brief 玩家退出
    void onLeave(const UserBaseDataPtr& userdata_self, const Ptr<protocol::app::PlayerLeave> &);

    /// @brief 玩家移动
    void onOtherMovement(const UserBaseDataPtr& userdata_self, const Ptr<protocol::app::OtherMovement> &othermove);
    void onSelfMovement (const UserBaseDataPtr& userdata_self, const Ptr<protocol::app::SelfMovement> &selfmove);

    /// @brief　玩家申请获取另一玩家的数据
    void onOtherPlayerDataRequest(const UserBaseDataPtr& userdata_self, const Ptr<protocol::app::OtherPlayerDataRequest> &request);

    /// @brief 玩家跳跃
    void onOtherJumpAndGravity(const UserBaseDataPtr& userdata_self, const Ptr<protocol::app::OtherJumpAndGravity> &otherJumpAndGravity);
    void onSelfJumpAndGravity(const UserBaseDataPtr& userdata_self, const Ptr<protocol::app::SelfJumpAndGravity> &selfJumpAndGravity);

    Ptr<yy::protocol::app::PlayerBaseData> FindPlayerByUID(UID_t onlineid);

    /// @brief 玩家`from`给其他玩家客户端转发数据`data`
    void Broadcast(const UserBaseDataPtr& from, const google::protobuf::Message & data);
    void Broadcast(const UserBaseDataPtr& from, const core::MessagePtr & data);

private:
    yy::core::IServer *                                                m_server;
    std::unordered_map<UID_t,  Ptr<yy::protocol::app::PlayerBaseData>> m_online_players;
    std::mutex m_online_players_mutex;

    yy::util::ObjectPool<yy::protocol::app::PlayerBaseData>            m_player_pool;
    int                                                                m_global_id;
};

}


#endif //____GAMEPLAYERMANAGER_H
