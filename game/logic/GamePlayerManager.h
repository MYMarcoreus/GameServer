#ifndef ____GAMEPLAYERMANAGER_H
#define ____GAMEPLAYERMANAGER_H

#include <unordered_map>
#include "IGameBase.h"
#include "Singleton.h"
#include "GameData.h"
#include "UnboundedLockedQueue.hpp"
#include "IServer.h"
#include "ObjectPool.h"
#include "core_definations.h"

using yy::core::UserConnectionPtr;

namespace yy::app {

class GamePlayerManager final: public Singleton<GamePlayerManager>
{
    SINGLETON_NECESSITY(GamePlayerManager)
public:
    void Init() ;

    // void StartListenAndIOLoop() override;
    void LeaveAndSave(UserConnectionPtr leave_user);

private:
    GamePlayerManager();
    ~GamePlayerManager() override;

    /// @brief 玩家发来登录请求，验证，然后将储存的游戏数据发送回玩家
    void OnEnterScene(const UserConnectionPtr& userdata, const Ptr<protocol::app::C2SEnterScene> & request);

    /// @brief 玩家退出
    void OnLeave(const UserConnectionPtr& userdata_self, const Ptr<protocol::app::C2SPlayerLeave> & leave);

    /// @brief 玩家移动
    void OnC2SMove (const UserConnectionPtr& userdata_self, const Ptr<protocol::app::C2SMove> &selfmove);

    /// @brief　玩家申请获取另一玩家的数据
    void OnC2SOtherPlayerData(const UserConnectionPtr& userdata_self, const Ptr<protocol::app::C2SOtherPlayerData> &request);

    /// @brief 玩家跳跃
    void OnC2SJumpAndGravity(const UserConnectionPtr& userdata_self, const Ptr<protocol::app::C2SJumpAndGravity> &selfJumpAndGravity);

    Ptr<yy::protocol::app::PlayerBaseData> FindPlayerByUID(UID_t onlineid);

    /// @brief 玩家`from`给其他玩家客户端转发数据`data`
    void Broadcast(const UserConnectionPtr& from, const google::protobuf::Message & data);
    void Broadcast(const UserConnectionPtr& from, const core::MessagePtr & data);

private:
    yy::core::IServer *                                                 m_server;
    std::unordered_map<UID_t,  uint64_t>                                m_uid_to_connid;
    std::unordered_map<UID_t,  Ptr<yy::protocol::app::PlayerBaseData>>  m_online_players;
    std::mutex                                                          m_online_players_mutex;

    yy::util::ObjectPool<yy::protocol::app::PlayerBaseData>             m_player_pool;
    int                                                                 m_global_id;
};

}


#endif //____GAMEPLAYERMANAGER_H

