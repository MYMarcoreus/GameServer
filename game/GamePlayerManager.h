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

    void Update() override;

    void AppCommand(const UserBaseDataPtr & userdata, int32_t cmd) override;

private:
    GamePlayerManager();
    ~GamePlayerManager() override;

    /// @brief 玩家发来登录请求，验证，然后将储存的游戏数据发送回玩家
    void onLogin(const UserBaseDataPtr& userdata);

    /// @brief 玩家退出
    void onLeave(const UserBaseDataPtr& userdata_self);

    /// @brief 玩家移动
    void onMove(const UserBaseDataPtr& userdata_self);

    /// @brief　玩家申请获取另一玩家的数据
    void onGetPlayerData(const UserBaseDataPtr& userdata_self);

    /// @brief 玩家跳跃
    void onJumpAndGravity(const UserBaseDataPtr& userdata_self);

    Ptr<protocol::PlayerBaseData> FindPlayerByUID(UID_t onlineid);

    /// @brief 玩家`from`给其他玩家客户端转发数据`data`
    void Broadcast(UID_t from, const google::protobuf::Message & data);

private:
    yy::core::IServer &                                       m_server;
    std::unordered_map<UID_t,  Ptr<protocol::PlayerBaseData>> m_online_players; // 无需上锁，因为这些都是在主线程完成的
    yy::util::ObjectPool<protocol::PlayerBaseData>            m_player_pool;
    int                                                       m_global_id;
};

}


#endif //____GAMEPLAYERMANAGER_H
