#pragma once

#include "GameData.h"
#include "game.pb.h"
#include "inner_room.pb.h"
#include "core_definations.h"
#include "ObjectPool.h"
#include "ProtobufDispatcher.h"
#include "RpcClient.hpp"

namespace yy::app::logic {
class LogicRedisDAO;

class GameService final : public protocol::app::LogicRoomServiceRpc
{
public:
    explicit GameService(net::EventLoop * baseLoop, core::IServer& frontend);
    ~GameService() override;

    //Region 内部Rpc通信
    void NewRoom(google::protobuf::RpcController* controller, const protocol::app::NewRoomReq* request,
                 protocol::app::NewRoomRsp* response, google::protobuf::Closure* done) override;

    void DeleteRoom(google::protobuf::RpcController* controller, const protocol::app::DeleteRoomReq* request,
                    protocol::app::DeleteRoomRsp* response, google::protobuf::Closure* done) override;

    void GetLogicAddr(google::protobuf::RpcController* controller, const protocol::app::GetLogicAddrReq* request,
        protocol::app::GetLogicAddrRsp* response, google::protobuf::Closure* done) override;
    //End
private:
    template<core::IsProtobufMessage MsgT, typename ClassT> requires core::MessageHandlerInvocable<ClassT, MsgT>
    void RegisterHandler(ClassT* self, core::ProtobufDispatcher<UserConnectionPtr>& dispatcher,
        void (ClassT::*handler)(const UserConnectionPtr&, const std::shared_ptr<MsgT>&));

    ///@brief 将消息加入房间对应的消息队列
    void DispatchMessage(const UserConnectionPtr& conn, const MessagePtr& msg) const;

    ///@brief 玩家离线，保存数据
    void OnPlayerDisconnect(const UserConnectionPtr& userconn);

    //Region 消息回调：玩家
    /// @brief 逻辑服登录请求
    void OnSceneLoginReq(const UserConnectionPtr& conn, const Ptr<protocol::app::SceneLoginReq> & req);
    //End

private:

private:
    net::EventLoop *                                        m_baseLoop;
    core::IServer&                                          m_frontend;
    std::unique_ptr<class RoomManager>                      m_roomManager;
    core::ProtobufDispatcher<UserConnectionPtr>             m_room_dispatcher;
    util::ObjectPool<protocol::app::PlayerBaseData> &       m_players_pool;
    LogicRedisDAO&                                          m_redisDAO;
};

template <core::IsProtobufMessage MsgT, typename ClassT> requires core::MessageHandlerInvocable<ClassT, MsgT>
void GameService::RegisterHandler(ClassT* self, core::ProtobufDispatcher<UserConnectionPtr>& dispatcher,
    void(ClassT::*handler)(const UserConnectionPtr&, const std::shared_ptr<MsgT>&))
{
    dispatcher.RegisterMessageCallback<MsgT>(
        [this, handler](const UserConnectionPtr& user, const std::shared_ptr<MsgT>& msg) {
            (this->*handler)(user, msg);
        }
    );
}
}
