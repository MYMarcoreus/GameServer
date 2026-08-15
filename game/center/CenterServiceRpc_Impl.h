#pragma once

#include "GateRpcClient.h"
#include "LogicServerController.h"
#include "LogicRpcClient.h"
#include "room.pb.h"


namespace yy::net { class EventLoop; }
namespace yy::core::rpc   { class RpcServer; }
namespace yy::core::redis { class RedisClient; }
namespace yy::core::mysql { class MySqlClient; }

namespace yy::app::center
{
class CenterRedisDAO;

class CenterServiceRpc_Impl final : public protocol::app::CenterRoomServiceRpc {

public:
    explicit CenterServiceRpc_Impl(net::EventLoop * base_loop);

    void CreateRoom(google::protobuf::RpcController* controller, const protocol::app::CreateRoomReq* request,
                    protocol::app::CreateRoomRsp* response, google::protobuf::Closure* done) override;

    void SearchRoom(google::protobuf::RpcController* controller, const protocol::app::SearchRoomReq* request,
                    protocol::app::SearchRoomRsp* response, google::protobuf::Closure* done) override;

    void SelfJoinRoom(google::protobuf::RpcController* controller, const protocol::app::SelfJoinRoomReq* request,
                      protocol::app::SelfJoinRoomRsp* response, google::protobuf::Closure* done) override;

    void SelfQuitRoom(google::protobuf::RpcController* controller, const protocol::app::SelfQuitRoomReq* request,
                      protocol::app::SelfQuitRoomRsp* response, google::protobuf::Closure* done) override;

    void GetEnterSceneToken(google::protobuf::RpcController* controller, const protocol::app::GetEnterSceneTokenReq* request,
                            protocol::app::GetEnterSceneTokenRsp* response, google::protobuf::Closure* done) override;

    void UserDisconnect(google::protobuf::RpcController* controller, const protocol::app::UserDisconnectReq* request,
                        protocol::app::UserDisconnectRsp* response, google::protobuf::Closure* done) override;

private:
    [[nodiscard]] LogicInfoController& get_logic_info_controller() const { return logic_controller_->get_logic_info_controller(); }
    [[nodiscard]] RoomInfoController& get_room_info_controller() const { return logic_controller_->get_room_info_controller(); }

    void Update();
    void BroadcastRoom(ROOM_ID_t room_id, core::UID_t from_uid, protocol::MessageCommand msg_cmd, std::string && msg_str);
    void BroadcastRoom(const RoomInfo& room, core::UID_t from_uid, protocol::MessageCommand msg_cmd, std::string && msg_str);
    static auto GenerateSceneToken() -> std::string;
    static auto GenerateRoomId() -> uint64_t;

    core::mysql::MySqlClient&                           mysql_pool_;
    std::unique_ptr<LogicServerController>              logic_controller_;
    CenterRedisDAO&                                     redis_dao_;
    rpc_client::LogicRpcClient&                         logic_rpc_client_;
    rpc_client::GateRpcClient&                          gate_rpc_client_;
};

}
