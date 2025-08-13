#include "CenterServiceRpc_Impl.h"

#include <uuid.h>

#include "CenterRedisDAO.h"
#include "CenterServerManager.h"
#include "ZkServiceClient.h"
#include "MySqlClient.h"
#include "RedisClient.h"
#include "CodecUtils.hpp"
#include "EventLoop.h"
#include "room.pb.h"

using namespace yy::protocol::app;
using namespace yy::net;
using namespace yy::core;
using namespace google::protobuf;

namespace yy::app::center
{
CenterServiceRpc_Impl::CenterServiceRpc_Impl(EventLoop * base_loop):
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

    logic_controller_ = std::make_unique<LogicServerController>(logic_rpc_client_.GetServiceName(), base_loop);
    logic_controller_->Init();

    base_loop->RunEvery(100ms, [this]
    {
        Update();
    });

    base_loop->RunEvery(1s, [this]
    {
        auto& room_controller = logic_controller_->get_room_info_controller();
        std::string server_str;
        for (auto& [server_name, server_info]: logic_controller_->get_logic_info_controller().GetAllServerInfo()) {
            server_str += std::format("{}-{}, ", server_info->get_name(), server_info->get_room_cnt());
        }
        YLOG_INFO("有 {} 个房间, 逻辑服有{}",  room_controller.RoomCount(), server_str);
        for (auto& [room_id, room]: room_controller.GetAllRoom()) {
            const auto room_data = room->get_room_data();
            YLOG_INFO("\t房间<{}:{}> of {}: {}/{} in {}-{}",
                room_id,
                room_data->name(),
                room_data->owner_uid(),
                room_data->exist_player_datas_size(),
                room_data->capacity(),
                room->get_server_info()->get_name(),
                room->get_server_info()->get_room_cnt()
            )
        }
    });
}

void CenterServiceRpc_Impl::Update()
{
    auto& room_controller = logic_controller_->get_room_info_controller();
    for (auto& [room_id, room]: room_controller.GetAllRoom()) {
        const auto is_del = room_controller.DelRoomIfEmpty(room_id);
        if (is_del) {
            YLOG_INFO("\t中心服删除房间<{}, {}>", room->get_room_id(), room->get_room_data()->name())

            // 通知逻辑服
            DeleteRoomReq del_req;
            del_req.set_room_id(room->get_room_id());
            logic_rpc_client_.CallRemoteAsync_From<DeleteRoomReq, DeleteRoomRsp>(room->get_server_info()->get_name(), del_req,
                [](std::unique_ptr<DeleteRoomRsp> && response, std::unique_ptr<rpc::RpcControllerImpl> && controller) {
                    if (response->success()) {
                        YLOG_INFO("\t逻辑服删除房间<{}>", response->room_id())
                    }
                });
        }
    }
}

void CenterServiceRpc_Impl::CreateRoom(RpcController* controller,
    const CreateRoomReq* request, CreateRoomRsp* response, Closure* done)
{
    YLOG_INFO("正在执行 CenterServiceRpc_Impl::CreateRoom 服务")
    //! 为房间分配服务器：按照房间的最小人数
    LogicServerInfoPtr server_info = logic_controller_->SelectLogicServer();
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
    YLOG_INFO("CenterServiceRpc_Impl::CreateRoom 服务：通告逻辑服创建房间")
    NewRoomReq req_NewRoom;
    req_NewRoom.mutable_room_data()->CopyFrom(room_data);
    req_NewRoom.set_user_token(request->user_token());
    //
    logic_rpc_client_.CallRemoteAsync_From<NewRoomReq, NewRoomRsp>(server_name,
        req_NewRoom,
        [this, response, done, room_data = std::move(room_data), server_info, owner_data = request->owner_data()]
        (std::unique_ptr<NewRoomRsp> && rsp_NewRoom, std::unique_ptr<rpc::RpcControllerImpl> && controller)
        {
            if(rsp_NewRoom->success()) {
                response->set_room_ip(rsp_NewRoom->ip());
                response->set_room_port(rsp_NewRoom->port());
                //! 中央服务器添加房间，并将创建者加入房间
                const RoomInfoPtr room = get_room_info_controller().AddRoom(room_data, server_info, owner_data);
                if (room) {
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

void CenterServiceRpc_Impl::SearchRoom(RpcController* controller,
    const SearchRoomReq* request, SearchRoomRsp* response, Closure* done)
{
    YLOG_INFO("正在执行 CenterServiceRpc_Impl::SearchRoom 服务")

    for (auto& room_data : get_room_info_controller().GetAllRoomData()) {
        response->add_room_datas()->CopyFrom(std::move(room_data));
    }
    response->set_uid(request->uid());

    done->Run();
}

void CenterServiceRpc_Impl::SelfJoinRoom(RpcController* controller,
    const SelfJoinRoomReq* request, SelfJoinRoomRsp* response, Closure* done)
{
    YLOG_INFO("正在执行 CenterServiceRpc_Impl::JoinRoom 服务：{}", request->ShortDebugString());

    response->set_uid(request->joinner_data().uid());

    auto [rst_code, room] = get_room_info_controller().AddPlayer(request->room_id(), request->joinner_data());
    switch (rst_code) {
    case RoomInfoController::AddPlayerResultCode::eSuccess: {
        if (!room)  break;
        // 加入成功，填写房间信息
        const auto room_addr = get_logic_info_controller().FindServerInfo(room->get_server_info()->get_name());
        response->set_room_ip(room_addr->get_ip());
        response->set_room_port(room_addr->get_port());
        response->set_result_code(SelfJoinRoomRsp_Status_eSuccess);
        response->mutable_room_data()->CopyFrom(*room->get_room_data());
        break;
    }
    case RoomInfoController::AddPlayerResultCode::eRoomNotExist:
        response->set_result_code(SelfJoinRoomRsp_Status_eRoomNotExist);
        break;
    case RoomInfoController::AddPlayerResultCode::eAlreadyJoined:
        response->set_result_code(SelfJoinRoomRsp_Status_eAlreadyJoined);
        break;
    case RoomInfoController::AddPlayerResultCode::eRoomFull:
        response->set_result_code(SelfJoinRoomRsp_Status_eRoomFull);
        break;
    case RoomInfoController::AddPlayerResultCode::eInternalError:
        response->set_result_code(SelfJoinRoomRsp_Status_eUnknownError);
        break;
    }
    // 发送响应
    done->Run();

    if (rst_code == RoomInfoController::AddPlayerResultCode::eSuccess) {
        // 发送广播
        OtherJoinRoomRsp msg;
        msg.mutable_joinner_data()->CopyFrom(request->joinner_data());
        msg.set_result_code(OtherJoinRoomRsp_Status_eSuccess);
        BroadcastRoom(request->room_id(), request->joinner_data().uid(), MSG_OtherJoinRoomRsp, msg.SerializeAsString());
    }
}

void CenterServiceRpc_Impl::SelfQuitRoom(RpcController* controller,
    const SelfQuitRoomReq* request, SelfQuitRoomRsp* response, Closure* done)
{
    YLOG_INFO("正在执行 CenterServiceRpc_Impl::QuitRoom 服务")

    // 响应请求方
    const auto [is_removed, room] = get_room_info_controller().DelPlayer(request->room_id(), request->uid());
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

void CenterServiceRpc_Impl::GetEnterSceneToken(RpcController* controller,
    const GetEnterSceneTokenReq* request, GetEnterSceneTokenRsp* response, Closure* done)
{
    YLOG_INFO("正在执行 CenterServiceRpc_Impl::GetEnterSceneToken 服务")

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

void CenterServiceRpc_Impl::UserDisconnect(RpcController* controller, const UserDisconnectReq* request,
    UserDisconnectRsp* response, Closure* done)
{
    YLOG_INFO("正在执行 CenterServiceRpc_Impl::UserDisconnect 服务")
    response->set_uid(request->uid());

    const auto [is_removed, room] = get_room_info_controller().DelPlayer(request->uid());
    done->Run();

    if (!room)
        return;

    // 发送广播
    OtherQuitRoomRsp msg;
    msg.set_result_code(OtherQuitRoomRsp_Status_eSuccess);
    msg.set_uid(response->uid());
    msg.set_room_id(room->get_room_id());
    BroadcastRoom(room->get_room_id(), request->uid(), MSG_OtherQuitRoomRsp, msg.SerializeAsString());
}

void CenterServiceRpc_Impl::BroadcastRoom(const ROOM_ID_t room_id, const UID_t from_uid, const MessageCommand msg_cmd, std::string && msg_str)
{
    const RoomInfoPtr room = get_room_info_controller().FindRoomByRoomID(room_id);
    if (!room) {
        YLOG_WARN("[CenterServiceRpc_Impl::BroadcastRoom] 找不到房间: {}", room_id);
        return;
    }
    BroadcastRoom(*room, from_uid, msg_cmd, std::move(msg_str));
}

void CenterServiceRpc_Impl::BroadcastRoom(const RoomInfo & room, const UID_t from_uid, MessageCommand msg_cmd, std::string&& msg_str)
{
    BroadcastRoomReq broadcast_req;
    broadcast_req.set_msg_cmd(msg_cmd);
    broadcast_req.set_room_id(room.get_room_id());

    for (auto& player_data : room.get_all_players()) {
        if (player_data.uid() == from_uid) {
            continue;
        }
        broadcast_req.add_target_uids(player_data.uid());
    }
    // 设置要广播的消息
    broadcast_req.set_payload(std::move(msg_str));

    // 发送给gate server
    gate_rpc_client_.CallRemoteAsync_Random<BroadcastRoomReq, BroadcastRoomRsp>(broadcast_req,
        [msg_cmd](std::unique_ptr<BroadcastRoomRsp> && response, std::unique_ptr<rpc::RpcControllerImpl> && controller) {
            YLOG_INFO("[CenterServiceRpc_Impl::BroadcastRoom] {}广播成功", g_cmd_to_name[msg_cmd])
        });
}


auto CenterServiceRpc_Impl::GenerateSceneToken() -> std::string
{
    return util::GenerateToken();
}

auto CenterServiceRpc_Impl::GenerateRoomId() -> uint64_t
{
    thread_local std::mt19937 engine{std::random_device{}()};
    thread_local uuids::uuid_random_generator gen{&engine};
    thread_local std::hash<uuids::uuid> hasher;
    const uuids::uuid uuid = gen();
    return hasher(uuid);
}




}
