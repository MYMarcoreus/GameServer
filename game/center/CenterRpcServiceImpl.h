#pragma once

#include "LogicServerController.h"
#include "LogicRpcClient.h"
#include "room.pb.h"


namespace yy::app::center
{
class CenterRedisDAO;
}

namespace yy::net { class EventLoop; }
namespace yy::core::rpc   { class RpcServer; }
namespace yy::core::redis { class RedisClient; }
namespace yy::core::mysql { class MySqlClient; }
namespace yy::core::zk    { class ZkServiceClient; }

namespace yy::app::center
{

class CenterRpcServiceImpl final : public protocol::app::CenterRoomServiceRpc {
public:
    explicit CenterRpcServiceImpl(EventLoop * base_loop);

    void CreateRoom(google::protobuf::RpcController* controller, const protocol::app::CreateRoomReq* request,
        protocol::app::CreateRoomRsp* response, google::protobuf::Closure* done) override;

    void SearchRoom(google::protobuf::RpcController* controller, const protocol::app::SearchRoomReq* request,
        protocol::app::SearchRoomRsp* response, google::protobuf::Closure* done) override;

    void JoinRoom(google::protobuf::RpcController* controller, const protocol::app::JoinRoomReq* request,
        protocol::app::JoinRoomRsp* response, google::protobuf::Closure* done) override;

    void QuitRoom(google::protobuf::RpcController* controller, const protocol::app::QuitRoomReq* request,
        protocol::app::QuitRoomRsp* response, google::protobuf::Closure* done) override;

    void GetEnterSceneToken(google::protobuf::RpcController* controller,
        const protocol::app::GetEnterSceneTokenReq* request, protocol::app::GetEnterSceneTokenRsp* response,
        google::protobuf::Closure* done) override;

private:
    auto GenerateSceneToken() -> std::string;
    auto GenerateRoomId() -> uint64_t;

    core::rpc::RpcServer&                               rpc_server_;
    rpc_client::LogicRpcClient&                         logic_client_;
    core::mysql::MySqlClient&                           mysql_pool_;
    std::unique_ptr<LogicServerController>              logic_controller_;
    CenterRedisDAO&                                     redis_dao_;
};

}
