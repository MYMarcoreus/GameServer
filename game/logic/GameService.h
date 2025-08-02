#pragma once

#include "GameData.h"
#include "game.pb.h"
#include "inner_room.pb.h"
#include "core_definations.h"
#include "ObjectPool.h"
#include "ProtobufDispatcher.h"
#include "Room.h"
#include "RpcClient.hpp"

namespace yy::net
{
class EventLoopThreadPool;
}

using yy::core::UserConnectionPtr;
using namespace yy::protocol::app;

namespace yy::app::logic {
class LogicRedisDAO;


class GameService final : public LogicRoomServiceRpc
{
public:
    explicit GameService(EventLoop * baseLoop);
    ~GameService() override;

    ///@brief 将消息加入房间对应的消息队列
    void PushMessage(UserConnectionPtr conn, core::MessagePtr msg);

    //Region 内部Rpc通信
    void NewRoom(google::protobuf::RpcController* controller, const NewRoomReq* request,
                 NewRoomRsp* response, google::protobuf::Closure* done) override;
    void DeleteRoom(google::protobuf::RpcController* controller, const DeleteRoomReq* request,
                    DeleteRoomRsp* response, google::protobuf::Closure* done) override;
    //End

    void SaveData(UID_t uid) {}

private:
    template<core::IsProtobufMessage MsgT, typename ClassT> requires core::MessageHandlerInvocable<ClassT, MsgT>
    void RegisterHandler(ClassT* self, core::ProtobufDispatcher<UserConnectionPtr>& dispatcher,
        void (ClassT::*handler)(const UserConnectionPtr&, const shared_ptr<MsgT>&));

    //Region 消息回调：玩家
    /// @brief 逻辑服登录请求
    void OnSceneLoginReq(const UserConnectionPtr& conn, const Ptr<SceneLoginReq> & req);

    /// @brief 玩家发来场景进入请求，然后将储存的游戏数据发送回玩家
    void OnEnterScene(const UserConnectionPtr& self_conn, const Ptr<C2SEnterScene> & req);

    /// @brief 玩家退出（如果是最后一个则销毁房间）
    void OnLeaveScene(const UserConnectionPtr& self_conn, const Ptr<C2SLeaveScene> & req);

    /// @brief 玩家移动
    void OnC2SMove (const UserConnectionPtr& self_conn, const Ptr<C2SMove> &selfmove);

    /// @brief 玩家申请获取另一玩家的数据
    void OnC2SOtherPlayerData(const UserConnectionPtr& self_conn, const Ptr<C2SOtherPlayerData> & req);

    /// @brief 玩家跳跃
    void OnC2SJumpAndGravity(const UserConnectionPtr& self_conn, const Ptr<C2SJumpAndGravity> &selfJumpAndGravity);
    //End

private:
    void UnkonwnCommand(const UserConnectionPtr & userdata, const core::MessagePtr & message)
    {
        YLOG_DEBUG("未知的消息类型：{}", message->GetDescriptor()->full_name())
        userdata->Shutdown();
    }

private:
    EventLoop *                                     m_baseLoop;
    core::IServer&                                  m_frontend_server;
    unique_ptr<EventLoopThreadPool>                 m_workThreads;
    unique_ptr<class RoomManager>                   m_roomManager;
    core::ProtobufDispatcher<UserConnectionPtr>     m_msgValidater;
    core::ProtobufDispatcher<UserConnectionPtr>     m_msgHandler; // 处理下层(core层)分发传来的无法处理的消息
    util::ObjectPool<PlayerBaseData> &              m_players_pool;
    LogicRedisDAO&                                  m_redisDAO;
};

template <core::IsProtobufMessage MsgT, typename ClassT> requires core::MessageHandlerInvocable<ClassT, MsgT>
void GameService::RegisterHandler(ClassT* self, core::ProtobufDispatcher<UserConnectionPtr>& dispatcher,
    void(ClassT::*handler)(const UserConnectionPtr&, const shared_ptr<MsgT>&))
{
    dispatcher.RegisterMessageCallback<MsgT>(
        [this, handler](const UserConnectionPtr& user, const shared_ptr<MsgT>& msg) {
            (this->*handler)(user, msg);
        }
    );
}
}
