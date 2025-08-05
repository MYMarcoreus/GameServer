#include "CenterRpcServiceImpl.h"

#include <uuid.h>

#include "CenterRedisDAO.h"
#include "CenterServerManager.h"
#include "ZkServiceClient.h"
#include "MySqlClient.h"
#include "RpcServer.h"
#include "RedisClient.h"
#include "CodecUtils.hpp"

using namespace yy::protocol::app;
using namespace yy::net;
using namespace yy::core;
using namespace google::protobuf;

namespace yy::app::center
{
CenterRpcServiceImpl::CenterRpcServiceImpl(EventLoop * base_loop):
    rpc_server_(CenterServerManager::Instance().GetRpcServer()),
    mysql_pool_(mysql::MySqlClient::Instance()),
    redis_dao_(CenterRedisDAO::Instance()),
    logic_rpc_client_(rpc_client::LogicRpcClient::Instance()),
    gate_rpc_client_{rpc_client::GateRpcClient::Instance()}
{
    // 初始化MySql
    mysql_pool_.Start(base_loop, "gameserver");
    // 初始化Redis
    redis_dao_.Start(base_loop);

    logic_rpc_client_.SetConnectionEstablishedCallback([](const TcpConnectionPtr & ) {
            YLOG_INFO("连接至LogicRpc服务器！");
        });
    gate_rpc_client_.SetConnectionEstablishedCallback(
        [](const TcpConnectionPtr & ) {
            YLOG_INFO("连接至GateRpc服务器！");
        });

    logic_controller_ = std::make_unique<LogicServerController>(logic_rpc_client_.GetServiceName());
    logic_controller_->Init();

    base_loop->RunEvery(100ms, [this]
    {
        Update();
    });

    base_loop->RunEvery(1s, [this]
    {
        auto& room_controller = logic_controller_->get_room_info_controller();
        YLOG_INFO("有 {} 个房间",  room_controller.RoomCount())
        for (auto& [room_id, room]: room_controller.GetAllRoom()) {
            const auto room_data = room->get_room_data();
            YLOG_INFO("\t房间<{}:{}> of {}: {}/{} in {}",
                room_id,
                room_data->name(),
                room_data->owner_uid(),
                room_data->exist_player_datas_size(),
                room_data->capacity(),
                room->get_server_info()->get_name()
            )
        }
    });
}

void CenterRpcServiceImpl::Update()
{
    auto& room_controller = logic_controller_->get_room_info_controller();
    for (auto& [room_id, room]: room_controller.GetAllRoom()) {
        const auto is_del = room_controller.DelRoomIfEmpty(room_id);
        if (is_del) {
            YLOG_INFO("\t删除房间<{}, {}>", room->get_room_id(), room->get_room_data()->name())
        }
    }
}

void CenterRpcServiceImpl::CreateRoom(RpcController* controller,
    const CreateRoomReq* request, CreateRoomRsp* response, Closure* done)
{
    YLOG_INFO("正在执行 CenterRpcServiceImpl::CreateRoom 服务")
    //! 为房间分配服务器：按照房间的最小人数
    const LogicServerInfoPtr server_info = logic_controller_->SelectLogicServer();
    if (server_info == nullptr) {
        response->set_result_code(CreateRoomRsp_Status_eNoServer);
        done->Run();
        return;
    }
    const auto server_name = server_info->get_name();
    const auto room_id = GenerateRoomId();

    RoomDetailData  room_data;
    room_data.set_room_id(room_id);
    room_data.set_capacity(request->capacity());
    room_data.set_name(request->name());
    room_data.set_owner_uid(request->owner_data().uid());
    response->set_uid(request->owner_data().uid());

    //! 通告逻辑服创建房间
    YLOG_INFO("CenterRpcServiceImpl::CreateRoom 服务：通告逻辑服创建房间")
    NewRoomReq req_NewRoom;
    req_NewRoom.mutable_room_data()->CopyFrom(room_data);
    req_NewRoom.set_user_token(request->user_token());
    //
    logic_rpc_client_.CallRemoteAsync_From<NewRoomReq, NewRoomRsp>(server_name,
        req_NewRoom,
        [this, response, done, room_data = std::move(room_data), server_info = std::move(server_info), owner_data = request->owner_data()]
        (std::unique_ptr<NewRoomRsp> && rsp_NewRoom, std::unique_ptr<rpc::RpcControllerImpl> && controller)
        {
            if(rsp_NewRoom->success()) {
                response->set_room_ip(rsp_NewRoom->ip());
                response->set_room_port(rsp_NewRoom->port());
                //! 中央服务器添加房间
                const RoomInfoPtr room = get_room_info_controller().AddRoom(room_data, server_info);
                //! 中央服务器添加玩家
                const auto is_added = room->AddPlayer(owner_data);
                if (is_added) {
                    response->set_result_code(CreateRoomRsp_Status_eSuccess);
                    response->mutable_room_data()->CopyFrom(*room->get_room_data());
                } else {
                    response->set_result_code(CreateRoomRsp_Status_eUnknownError);
                }
            } else {
                response->set_result_code(CreateRoomRsp_Status_eUnknownError);
            }
            YLOG_INFO("收到NewRoomRsp：{}", rsp_NewRoom->DebugString())
            done->Run();
        });
}

void CenterRpcServiceImpl::SearchRoom(RpcController* controller,
    const SearchRoomReq* request, SearchRoomRsp* response, Closure* done)
{
    YLOG_INFO("正在执行 CenterRpcServiceImpl::SearchRoom 服务")

    for (auto& room_data : get_room_info_controller().GetAllRoomData()) {
        response->add_room_datas()->CopyFrom(std::move(room_data));
    }
    response->set_uid(request->uid());

    done->Run();
}

void CenterRpcServiceImpl::SelfJoinRoom(RpcController* controller,
    const SelfJoinRoomReq* request, SelfJoinRoomRsp* response, Closure* done)
{
    YLOG_INFO("正在执行 CenterRpcServiceImpl::JoinRoom 服务");

    const auto uid = request->joinner_data().uid();
    const auto room_id = request->room_id();
    response->set_uid(uid);

    const auto room = get_room_info_controller().FindRoomByRoomID(room_id);
    if (!room) {
        response->set_result_code(SelfJoinRoomRsp_Status_eRoomNotExist);
        done->Run();
        return;
    }

    if (get_room_info_controller().IsJoined(room_id, uid)) {
        response->set_result_code(SelfJoinRoomRsp_Status_eUnknownError);
        done->Run();
        return;
    }

    if (!room->AddPlayer(request->joinner_data())) {
        response->set_result_code(SelfJoinRoomRsp_Status_eUnknownError);
        done->Run();
        return;
    }

    // 加入成功，填写房间信息
    const auto room_addr = get_logic_info_controller().FindServerInfo(room->get_server_info()->get_name());
    response->set_room_ip(room_addr->get_ip());
    response->set_room_port(room_addr->get_port());
    response->set_result_code(SelfJoinRoomRsp_Status_eSuccess);
    response->mutable_room_data()->CopyFrom(*room->get_room_data());
    done->Run();

    // 发送广播
    OtherJoinRoomRsp msg;
    msg.mutable_joinner_data()->CopyFrom(request->joinner_data());
    msg.set_result_code(OtherJoinRoomRsp_Status_eSuccess);
    BroadcastRoom(request->room_id(), request->joinner_data().uid(), MSG_OtherJoinRoomRsp, msg.SerializeAsString());
}

void CenterRpcServiceImpl::SelfQuitRoom(RpcController* controller,
    const SelfQuitRoomReq* request, SelfQuitRoomRsp* response, Closure* done)
{
    YLOG_INFO("正在执行 CenterRpcServiceImpl::QuitRoom 服务")

    // 响应请求方
    const bool is_removed = get_room_info_controller().DelPlayer(request->room_id(), request->uid());
    if(not is_removed) {
        response->set_result_code(SelfQuitRoomRsp_Status_eUnknownError);
        YLOG_INFO("玩家退出房间失败");
        done->Run();
        return;
    }
    response->set_result_code(SelfQuitRoomRsp_Status_eSuccess);
    response->set_room_id(request->room_id());
    response->set_uid(request->uid());
    done->Run();

    // 发送广播
    OtherQuitRoomRsp msg;
    msg.set_result_code(OtherQuitRoomRsp_Status_eSuccess);
    msg.set_uid(response->uid());
    msg.set_room_id(request->room_id());
    BroadcastRoom(request->room_id(), request->uid(), MSG_OtherQuitRoomRsp, msg.SerializeAsString());
}

void CenterRpcServiceImpl::GetEnterSceneToken(RpcController* controller,
    const GetEnterSceneTokenReq* request, GetEnterSceneTokenRsp* response, Closure* done)
{
    YLOG_INFO("正在执行 CenterRpcServiceImpl::GetEnterSceneToken 服务")

    auto scene_token = GenerateSceneToken();
    // 将SceneToken存入redis并设置较短的过期时间，逻辑服收到玩家进入场景的请求时获取并删除该token
    redis_dao_.SetSceneTokenWithExpire(request->uid(), scene_token, 60s);

    // 发送响应
    response->set_result_code(GetEnterSceneTokenRsp_Status_eSuccess);
    response->set_uid(request->uid());
    response->set_room_id(request->room_id());
    response->set_scene_token(scene_token);
    done->Run();
}

void CenterRpcServiceImpl::UserDisconnect(RpcController* controller, const UserDisconnectReq* request,
    UserDisconnectRsp* response, Closure* done)
{
    YLOG_INFO("正在执行 CenterRpcServiceImpl::UserDisconnect 服务")
    response->set_uid(request->uid());

    const auto room = get_room_info_controller().FindRoomByUID(request->uid());
    if(room == nullptr) {
        done->Run();
        return;
    }
    const bool is_removed = room->DelPlayer(request->uid());
    if(not is_removed){
        done->Run();
        return;
    }

    // 发送广播
    OtherQuitRoomRsp msg;
    msg.set_result_code(OtherQuitRoomRsp_Status_eSuccess);
    msg.set_uid(response->uid());
    msg.set_room_id(room->get_room_id());
    BroadcastRoom(room->get_room_id(), request->uid(), MSG_OtherQuitRoomRsp, msg.SerializeAsString());
}

void CenterRpcServiceImpl::BroadcastRoom(const ROOM_ID_t room_id, const UID_t from_uid, const MessageCommand msg_cmd, std::string && msg_str)
{
    BroadcastRoomReq broadcast_req;
    broadcast_req.set_msg_cmd(msg_cmd);
    broadcast_req.set_room_id(room_id);
    const RoomInfoPtr room = get_room_info_controller().FindRoomByRoomID(room_id);
    if (!room) {
        YLOG_WARN("BroadcastRoom 找不到房间: {}", room_id);
        return;
    }
    for (auto& player_data : room->get_all_players()) {
        if (player_data.uid() == from_uid) {
            continue;
        }
        broadcast_req.add_target_uids(player_data.uid());
    }
    // 设置要广播的消息
    broadcast_req.set_payload(std::move(msg_str));

    // 发送给gate server
    gate_rpc_client_.CallRemoteAsync_Random<BroadcastRoomReq, BroadcastRoomRsp>(broadcast_req,
        [](std::unique_ptr<BroadcastRoomRsp> && response, std::unique_ptr<rpc::RpcControllerImpl> && controller) {
            YLOG_INFO("SelfQuitRoom广播成功")
        });
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
