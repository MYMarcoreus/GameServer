#pragma once

#include "GameData.h"
#include "core_definations.h"
#include "ObjectPool.h"
#include "Room.h"

using yy::core::UserConnectionPtr;
using namespace yy::protocol::app;

namespace yy::app::logic {


class RoomService final
{
public:
    RoomService();
    ~RoomService();

    void Init();

    void LeaveAndSave(const UserConnectionPtr& leave_conn);

private:
    //Region 消息回调
    /// @brief 玩家发来场景进入请求，然后将储存的游戏数据发送回玩家
    void OnEnterRoom(const UserConnectionPtr& self_conn, const Ptr<C2SEnterRoom> & request);

    /// @brief 玩家退出
    void OnLeaveRoom(const UserConnectionPtr& self_conn, const Ptr<C2SLeaveRoom> & leave);

    /// @brief 玩家移动
    void OnC2SMove (const UserConnectionPtr& self_conn, const Ptr<C2SMove> &selfmove);

    /// @brief 玩家申请获取另一玩家的数据
    void OnC2SOtherPlayerData(const UserConnectionPtr& self_conn, const Ptr<C2SOtherPlayerData> &request);

    /// @brief 玩家跳跃
    void OnC2SJumpAndGravity(const UserConnectionPtr& self_conn, const Ptr<C2SJumpAndGravity> &selfJumpAndGravity);
    //End

private:
    PlayerPtr FindPlayerByUID(UID_t uid);

    void InitPlayerData(const PlayerBaseDataPtr & data, uint64_t uid);

    PlayerPtr CreatePlayer(const UserConnectionPtr& conn, const PlayerBaseDataPtr& data);

private:
    core::IServer&                      server_;
    Room                                players_; // todo 多个房间
    util::ObjectPool<PlayerBaseData> &  players_pool_;
};

}
