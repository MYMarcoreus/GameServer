#pragma once
#include <chrono>

#include "GameData.h"
#include "net_definations.h"
#include "ObjectPool.h"
#include "Player.h"
#include "ProtobufDispatcher.h"
#include "room_data.pb.h"
#include "Actor.h"

namespace yy::net { class EventLoop; }


namespace yy::app::logic
{

// 管理一个房间内的玩家：一个房间在一个线程中处理，线程安全，无锁（基于 Actor 框架）
class Room: public core::actor::Actor<Room>{
public:
    //! 房间空闲检测与超时销毁（防止僵尸房间）
    static constexpr std::chrono::seconds ROOM_IDLE_CHECK_INTERVAL{60};
    static constexpr std::chrono::minutes ROOM_IDLE_TIMEOUT{30};

    Room(net::EventLoop * loop, const protocol::app::RoomDetailData& data,
         std::function<void(UID_t)> on_player_remove,
         std::function<void(ROOM_ID_t)> on_room_stop);

    ~Room();

    void PostMessage(const UserConnectionPtr& conn, const MessagePtr& msg);

    void PostTask(const net::F_TaskCallback& task);

    void OnPlayerDisconnect(const UserConnectionPtr& userconn);

    //Region Getter
    [[nodiscard]] auto get_room_data() const -> protocol::app::RoomDetailData ;
    [[nodiscard]] auto get_owner_uid() const -> UID_t ;
    [[nodiscard]] auto get_name() const -> const std::string& ;
    [[nodiscard]] auto get_id() const -> ROOM_ID_t ;
    [[nodiscard]] auto get_capacity() const -> int ;
    //End

    //Region 暴露给外部的接口，需要在 Room 线程内调用（通过 Send/AskWith 投递）
    /// @brief 加入玩家；返回是否成功（重复加入或对象池满则失败）
    [[nodiscard]] bool AddPlayer(const UserConnectionPtr& self_conn, protocol::app::AccountBaseData account_data);
    //End

protected:
    void OnStart() override;

    void OnStop() override;

private:
    [[nodiscard]] auto FindPlayer(UID_t uid) -> PlayerPtr;
    [[nodiscard]] auto GetAllPlayers() const -> const std::unordered_map<UID_t, PlayerPtr>&;
    [[nodiscard]] auto RemovePlayer(UID_t uid) -> PlayerPtr;
    void InitPlayerData(const PlayerBaseDataPtr& self_data, protocol::app::AccountBaseData account_data);

    //Region 房间广播
    void BroadcastUDP(const PlayerPtr& from, const google::protobuf::Message &data);
    void BroadcastUDP(const PlayerPtr& from, const MessagePtr &data);
    void BroadcastUDP(UID_t from_uid, const google::protobuf::Message &data);
    void BroadcastTCP(const PlayerPtr& from, const google::protobuf::Message &data);
    void BroadcastTCP(const PlayerPtr& from, const MessagePtr &data);
    void BroadcastTCP(UID_t from_uid, const google::protobuf::Message &data);
    //End

    //Region 客户端消息注册
    template <core::IsProtobufMessage MsgT, typename ClassT> requires core::MessageHandlerInvocable<ClassT, MsgT>
    void RegisterHandler(ClassT* self, core::ProtobufDispatcher<UserConnectionPtr>& dispatcher,
        void(ClassT::*handler)(const UserConnectionPtr&, const std::shared_ptr<MsgT>&))
    {
        dispatcher.RegisterMessageCallback<MsgT>(
            [this, handler](const UserConnectionPtr& user, const std::shared_ptr<MsgT>& msg) {
                (this->*handler)(user, msg);
            }
        );
    }
    //End

    //Region 客户端消息
    /// @brief 玩家发来场景进入请求，然后将储存的游戏数据发送回玩家
    void OnEnterScene(const UserConnectionPtr& self_conn, const Ptr<protocol::app::C2SEnterScene> & req);

    /// @brief 玩家退出
    void OnLeaveScene(const UserConnectionPtr& self_conn, const Ptr<protocol::app::C2SLeaveScene> & req);

    /// @brief 玩家移动
    void OnC2SMove(const UserConnectionPtr& self_conn, const Ptr<protocol::app::C2SMove> &selfmove);

    /// @brief 玩家申请获取另一玩家的数据
    void OnC2SOtherPlayerData(const UserConnectionPtr& self_conn, const Ptr<protocol::app::C2SOtherPlayerData> & req);

    /// @brief 玩家跳跃
    void OnC2SJumpAndGravity(const UserConnectionPtr& self_conn, const Ptr<protocol::app::C2SJumpAndGravity> &selfJumpAndGravity);
    //End

private:
    protocol::app::RoomDetailData                       room_data_;
    std::unordered_map<UID_t, PlayerPtr>                players_;
    core::ProtobufDispatcher<UserConnectionPtr>         msg_handler_;
    util::ObjectPool<protocol::app::PlayerBaseData> &   players_pool_;
    std::function<void(UID_t)> m_PlayerRemoveCb;
    std::function<void(ROOM_ID_t)> m_RoomStopCb;
    std::chrono::steady_clock::time_point               last_activity_{std::chrono::steady_clock::now()};
};

using RoomPtr = std::shared_ptr<Room>;

}
