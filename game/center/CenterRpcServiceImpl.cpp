#include "CenterRpcServiceImpl.h"

#include <uuid.h>

#include "CenterRedisDAO.h"
#include "CenterServerManager.h"
#include "ZkServiceClient.h"
#include "MySqlClient.h"
#include "RpcServer.h"
#include "RedisClient.h"

using namespace yy::protocol::app;

namespace yy::app::center
{
CenterRpcServiceImpl::CenterRpcServiceImpl(EventLoop * base_loop):
    rpc_server_(CenterServerManager::Instance().GetRpcServer()),
    logic_client_(rpc_client::LogicRpcClient::Instance()),
    mysql_pool_(mysql::MySqlClient::Instance()),
    redis_dao_(CenterRedisDAO::Instance())
{
    // 初始化MySql
    mysql_pool_.Start(base_loop, "gameserver");
    // 初始化Redis
    redis_dao_.Start(base_loop);
    // 阻塞连接所有逻辑服
    logic_client_.Start(
        // 3,
        [](const TcpConnectionPtr & ) {
        YLOG_INFO("连接至LogicRpc服务器！");
    });
    logic_controller_ = std::make_unique<LogicServerController>(logic_client_.GetServiceName());
    logic_controller_->Init();
}

void CenterRpcServiceImpl::CreateRoom(google::protobuf::RpcController* controller,
    const CreateRoomReq* request, CreateRoomRsp* response,
    google::protobuf::Closure* done)
{
    YLOG_INFO("正在执行 CenterRpcServiceImpl::CreateRoom 服务")
    //! 为房间分配服务器：按照房间的最小人数
    const auto server_info = logic_controller_->SelectLogicServer();
    if (server_info == nullptr) {
        response->set_result_code(CreateRoomRsp_Status_eNoServer);
        done->Run();
        return;
    }
    const auto server_name = std::format("{}:{}", server_info->get_ip(), server_info->get_port());
    const auto room_id = GenerateRoomId();

    response->set_uid(request->owner_data().uid());
    RoomBriefData* brief_data = response->mutable_room_data()->mutable_breif_data();
    brief_data->set_room_id(room_id);
    brief_data->set_capacity(request->capacity());
    brief_data->set_name(request->name());
    brief_data->set_owner_uid(request->owner_data().uid());

    //! 通告逻辑服创建房间
    YLOG_INFO("CenterRpcServiceImpl::CreateRoom 服务：通告逻辑服创建房间")
    NewRoomReq req_NewRoom;
    req_NewRoom.mutable_room_data()->mutable_breif_data()->CopyFrom(*brief_data);
    req_NewRoom.set_user_token(request->user_token());
    logic_client_.CallRemoteAsync_From<NewRoomReq, NewRoomRsp>(server_name,
        req_NewRoom,
        [this, response, done, server_name, owner_data = request->owner_data()](std::unique_ptr<NewRoomRsp> && rsp_NewRoom, std::unique_ptr<rpc::RpcControllerImpl> && controller)
        {
            if(rsp_NewRoom->success()) {
                response->set_result_code(CreateRoomRsp_Status_eSuccess);
                response->set_room_ip(rsp_NewRoom->ip());
                response->set_room_port(rsp_NewRoom->port());

                //! 中央服务器添加房间
                RoomInfoController& room_controller = this->logic_controller_->get_room_info_controller();
                const RoomInfoPtr room = room_controller.AddRoom(response->room_data(), server_name);
                //! 中央服务器添加玩家
                room_controller.AddPlayer(room, owner_data);
                (void)0;
            } else {
                response->set_result_code(CreateRoomRsp_Status_eUnknownError);
            }
            YLOG_INFO("收到NewRoomRsp：{}", rsp_NewRoom->DebugString())
            done->Run();
        });
}

void CenterRpcServiceImpl::SearchRoom(google::protobuf::RpcController* controller,
    const SearchRoomReq* request, SearchRoomRsp* response,
    google::protobuf::Closure* done)
{
    YLOG_INFO("正在执行 CenterRpcServiceImpl::SearchRoom 服务")

    response->set_uid(request->uid());
    //todo

    done->Run();
}

void CenterRpcServiceImpl::JoinRoom(google::protobuf::RpcController* controller,
    const JoinRoomReq* request, JoinRoomRsp* response,
    google::protobuf::Closure* done)
{
    YLOG_INFO("正在执行 CenterRpcServiceImpl::JoinRoom 服务")

    response->set_result_code(JoinRoomRsp_Status_eSuccess);
    //todo

    done->Run();
}

void CenterRpcServiceImpl::QuitRoom(google::protobuf::RpcController* controller,
    const QuitRoomReq* request, QuitRoomRsp* response, google::protobuf::Closure* done)
{
    YLOG_INFO("正在执行 CenterRpcServiceImpl::QuitRoom 服务")

    RoomInfoController& room_controller = this->logic_controller_->get_room_info_controller();
    const auto is_removed = room_controller.DelPlayer(request->room_id(), request->uid());
    if(is_removed) {
        response->set_result_code(QuitRoomRsp_Status_eSuccess);
    } else {
        response->set_result_code(QuitRoomRsp_Status_eUnknownError);
    }
    response->set_room_id(request->room_id());
    response->set_uid(request->uid());

    done->Run();
}

void CenterRpcServiceImpl::GetEnterSceneToken(google::protobuf::RpcController* controller,
    const GetEnterSceneTokenReq* request, GetEnterSceneTokenRsp* response, google::protobuf::Closure* done)
{
    YLOG_INFO("正在执行 CenterRpcServiceImpl::GetEnterSceneToken 服务")

    auto scene_token = GenerateSceneToken();
    //todo 将SceneToken存入redis并设置较短的过期时间，逻辑服收到玩家进入场景的请求时获取并删除该token
    redis_dao_.SetSceneTokenWithExpire(request->uid(), scene_token, 60s);

    response->set_result_code(GetEnterSceneTokenRsp_Status_eSuccess);
    response->set_uid(request->uid());
    response->set_room_id(request->room_id());
    response->set_scene_token(scene_token);
    done->Run();
}


auto CenterRpcServiceImpl::GenerateSceneToken() -> std::string
{
    return util::GenerateToken();
}

auto CenterRpcServiceImpl::GenerateRoomId() -> uint64_t
{
    thread_local std::mt19937 engine{std::random_device{}()};
    thread_local uuids::uuid_random_generator gen{&engine};
    thread_local std::hash<uuids::uuid> hasher;
    const uuids::uuid uuid = gen();
    return hasher(uuid);
}




}
