#pragma once
#include "GameData.h"
#include "net_definations.h"
#include "ObjectPool.h"
#include "Player.h"
#include "ProtobufDispatcher.h"
#include "room_data.pb.h"

namespace yy::net { class EventLoop; }


namespace yy::app::logic
{

using ROOM_ID_t = uint64_t;

// 管理一个房间内的玩家：一个房间在一个线程中处理，线程安全，无锁
class Room: public std::enable_shared_from_this<Room>{
public:
    Room(net::EventLoop * loop, const protocol::app::RoomDetailData& data);

    ~Room();

    void Init(net::Milliseconds deltaTime);

    void PostMessage(const core::UserConnectionPtr& conn, const core::MessagePtr& msg) const;

    void PostTask(const net::F_TaskCallback& task) const;

    void OnPlayerDisconnect(const core::UserConnectionPtr& userconn);

    //Region Getter
    [[nodiscard]] auto get_room_data() const -> protocol::app::RoomDetailData { return room_data_; }
    [[nodiscard]] auto get_owner_uid() const -> core::UID_t { return room_data_.owner_uid(); }
    [[nodiscard]] auto get_name() const -> const std::string& { return room_data_.name(); }
    [[nodiscard]] auto get_id() const -> ROOM_ID_t { return room_data_.room_id(); }
    [[nodiscard]] auto get_capacity() const -> int { return room_data_.capacity(); }
    //End

    //Region 暴露给外部的接口，需要将任务投递给Room所在线程
    void AddPlayer(const core::UserConnectionPtr& self_conn, protocol::app::AccountBaseData account_data);
    [[nodiscard]] auto GetAllPlayers() -> std::unordered_map<core::UID_t, PlayerPtr>;
    //End

private:
    void Update();

    void StopUpdate();

    [[nodiscard]] auto FindPlayer(core::UID_t uid) -> PlayerPtr;
    [[nodiscard]] auto RemovePlayer(core::UID_t uid) -> PlayerPtr;
    void InitPlayerData(const PlayerBaseDataPtr& self_data, protocol::app::AccountBaseData account_data);

    //Region 房间广播
    void Broadcast(const PlayerPtr& from, const google::protobuf::Message &data);
    void Broadcast(const PlayerPtr& from, const core::MessagePtr &data);
    void Broadcast(core::UID_t from_uid, const google::protobuf::Message &data);
    //End

    //Region 客户端消息注册
    template <core::IsProtobufMessage MsgT, typename ClassT> requires core::MessageHandlerInvocable<ClassT, MsgT>
    void RegisterHandler(ClassT* self, core::ProtobufDispatcher<core::UserConnectionPtr>& dispatcher,
        void(ClassT::*handler)(const core::UserConnectionPtr&, const std::shared_ptr<MsgT>&))
    {
        dispatcher.RegisterMessageCallback<MsgT>(
            [this, handler](const core::UserConnectionPtr& user, const std::shared_ptr<MsgT>& msg) {
                (this->*handler)(user, msg);
            }
        );
    }
    //End

    //Region 客户端消息
    /// @brief 玩家发来场景进入请求，然后将储存的游戏数据发送回玩家
    void OnEnterScene(const core::UserConnectionPtr& self_conn, const Ptr<protocol::app::C2SEnterScene> & req);

    /// @brief 玩家退出（如果是最后一个则销毁房间）
    void OnLeaveScene(const core::UserConnectionPtr& self_conn, const Ptr<protocol::app::C2SLeaveScene> & req);

    /// @brief 玩家移动
    void OnC2SMove(const core::UserConnectionPtr& self_conn, const Ptr<protocol::app::C2SMove> &selfmove);

    /// @brief 玩家申请获取另一玩家的数据
    void OnC2SOtherPlayerData(const core::UserConnectionPtr& self_conn, const Ptr<protocol::app::C2SOtherPlayerData> & req);

    /// @brief 玩家跳跃
    void OnC2SJumpAndGravity(const core::UserConnectionPtr& self_conn, const Ptr<protocol::app::C2SJumpAndGravity> &selfJumpAndGravity);
    //End

private:
    net::EventLoop *                                        loop_;
    net::TimerID                                            update_timer_id_;
    protocol::app::RoomDetailData                           room_data_;
    std::unordered_map<core::UID_t, PlayerPtr>              players_;
    core::ProtobufDispatcher<core::UserConnectionPtr>       msg_handler_;
    util::ObjectPool<protocol::app::PlayerBaseData> &       players_pool_;
};

using RoomPtr = std::shared_ptr<Room>;

}
