#include "GameService.h"

#include "EventLoopThreadPool.h"
#include "LogicServerManager.h"
#include "log.h"
#include "UserConnection.h"
#include "IServer.h"
#include "RoomManager.h"
#include "EventLoop.h"
#include "LogicRedisDAO.h"


using namespace yy::core;
using namespace yy::util;
using namespace yy::protocol::app;

namespace yy::app::logic {
GameService::GameService(EventLoop * baseLoop):
    m_baseLoop{baseLoop},
    m_frontend_server{LogicServerManager::Instance().GetServer()},
    m_msgValidater{[this](const UserConnectionPtr& conn, const MessagePtr& msg) {
        const RoomPtr room = m_roomManager->FindRoomByUID(conn->GetUID());
        assert(room);

        //! 即时处理（非Update）：对于游戏消息，并不在IO线程处理，而是在专门处理游戏数据的工作线程中处理（让分发器找到该游戏消息所注册的对应的处理函数。）
        room->GetLoop()->RunCallbackInLoop([this, conn, msg] { // 注意这里跨线程传输需要拷贝智能指针
            // 调用消息对应的处理函数
            m_msgHandler.OnProtobufMessage(conn, msg);
        });
    }},
    m_msgHandler{[this](const UserConnectionPtr& userdata, const MessagePtr& message) { this->UnkonwnCommand(userdata, message); }},
    m_players_pool{ObjectPool<PlayerBaseData>::Instance()},
    m_redisDAO{LogicRedisDAO::Instance()}
{
    m_frontend_server.SetNotifier_Command(
        [this](const UserConnectionPtr & userdata, const MessagePtr & message, const MessageNetType type) {
            this->PushMessage(userdata, message);
        });

    RegisterHandler(this, this->m_msgValidater, &GameService::OnSceneLoginReq);
    RegisterHandler(this, this->m_msgHandler  , &GameService::OnEnterScene);
    RegisterHandler(this, this->m_msgHandler  , &GameService::OnLeaveScene);
    RegisterHandler(this, this->m_msgHandler  , &GameService::OnC2SOtherPlayerData);
    RegisterHandler(this, this->m_msgHandler  , &GameService::OnC2SMove);
    RegisterHandler(this, this->m_msgHandler  , &GameService::OnC2SJumpAndGravity);

    m_players_pool.Init("玩家对象池", m_frontend_server.GetAppConfig().app_player_max(), nullptr, nullptr);
    m_workThreads = std::make_unique<EventLoopThreadPool>(m_baseLoop);
    m_workThreads->Start(config::g_app_config->GetValue().work_thread_num(), 500ms); //! 启动服务器的工作线程：即时处理
    m_roomManager = std::make_unique<RoomManager>();
    m_redisDAO.Start(m_baseLoop);
}

GameService::~GameService() = default;

void GameService::NewRoom(google::protobuf::RpcController* controller, const NewRoomReq* request, NewRoomRsp* response,
    google::protobuf::Closure* done)
{
    m_baseLoop->RunCallbackInLoop([this, room_data = request->room_data(), response, done] {
        //! 新建房间但是不新建Player，需要在客户端连接logic server时再AddPlayer
        YLOG_INFO("正在执行 GameService::NewRoom 服务: {}", room_data.ShortDebugString())
        const RoomPtr new_room = m_roomManager->AddRoom(m_workThreads->GetNextLoop() /* 分配房间对应的线程 */, room_data);

        response->set_success(true);
        response->set_room_id(new_room->get_id());
        // 给出逻辑服对外开放的ip和端口
        const auto fronend_addr = m_frontend_server.GetListenAddr();
        response->set_ip(fronend_addr->GetIPStr());
        response->set_port(fronend_addr->GetPort());

        // 发送响应
        done->Run();
        YLOG_INFO("执行完毕 GameService::NewRoom 服务: {}", room_data.ShortDebugString())
    });
}

void GameService::DeleteRoom(google::protobuf::RpcController* controller, const DeleteRoomReq* request,
    DeleteRoomRsp* response, google::protobuf::Closure* done)
{
    YLOG_INFO("正在执行 GameService::DeleteRoom 服务: {}", request->ShortDebugString())
    // 删除房间
    const bool is_removed = m_roomManager->RemoveRoom(request->room_id());

    // 填写响应
    response->set_success(is_removed);
    response->set_room_id(request->room_id());

    // 发送响应
    done->Run();
    YLOG_INFO("正在执行 GameService::DeleteRoom 服务: {}", request->ShortDebugString())
}

void GameService::PushMessage(UserConnectionPtr conn, MessagePtr msg)
{
    YLOG_TRACE("收到客户端消息{}：{}", msg->GetDescriptor()->DebugString(), msg->DebugString())
    m_msgValidater.OnProtobufMessage(conn, msg);
}


void GameService::OnSceneLoginReq(const UserConnectionPtr& conn, const Ptr<SceneLoginReq>& req)
{
    // 验证登录
    const auto scene_token = m_redisDAO.GetAndDelSceneToken(req->uid());
    const auto user_token  = m_redisDAO.GetUserTokenAndRefreshEx(req->uid());

    SceneLoginRsp resp;
    bool is_ok;
    if (not conn->IsLoggedIn() and scene_token and user_token and req->scene_token() == scene_token and user_token == req->user_token()) {
        is_ok = true;
        // 设置登录状态
        conn->SetUID(req->uid());
        conn->SetToken(req->user_token());
        conn->SetState(UserConnection::E_UserBaseState::eLoggedIn);
    } else {
        is_ok = false;
        YLOG_ERROR("验证token失败！");
    }

    resp.set_is_ok(is_ok);
    conn->SendTCP(resp);
}

void GameService::OnEnterScene(const UserConnectionPtr& self_conn, const Ptr<protocol::app::C2SEnterScene> & req) //NOLINT
{
    auto SendEnterSceneFailed = [&](std::string_view reason) {
        YLOG_ERROR("进入场景失败: {}", reason);
        S2CEnterScene resp;
        resp.set_result(false);
        self_conn->SendTCP(resp);
    };

    // 验证房间
    const auto room = m_roomManager->FindRoomByRoomID(req->room_id());
    if(room == nullptr) {
        SendEnterSceneFailed("房间不存在");
        return;
    }

    // 初始化进入玩家对象，加入玩家数据列表
    self_conn->SetUID(req->uid());
    const auto self_data = m_players_pool.Acquire(2s);
    if(self_data == nullptr) {
        SendEnterSceneFailed("玩家对象池资源不足，不准进入场景！");
        return;
    }
    room->InitPlayerData(self_data, req->uid());
    const auto self_player = room->AddPlayer(self_conn, self_data);

    // 进入请求：填充其他玩家数据
    S2CEnterScene loginResponse;
    loginResponse.set_uid(req->uid());
    loginResponse.set_room_id(req->room_id());
    for (const PlayerPtr & other_player : room->GetAllPlayers() | std::views::values) {
        if(other_player->GetUID() == self_player->GetUID())
            continue;
        // add_othersdata为repeated字段增加元素，返回值就是该元素的指针
        PlayerBaseData* other_data = loginResponse.add_other_datas();
        other_data->CopyFrom(*other_player->GetBaseData()); // 复制
    }
    loginResponse.set_result(true);
    loginResponse.mutable_self_data()->CopyFrom(*self_data);
    YLOG_INFO("发送loginResponse: {}", loginResponse.DebugString());

    // 返回给登录用户自己的信息和其他人的信息
    self_conn->SendTCP(loginResponse);

    // 返回登录用户的信息给其他用户
    S2COtherPlayerData otherPlayerDataResponse;
    otherPlayerDataResponse.set_uid(req->uid());
    otherPlayerDataResponse.set_room_id(req->room_id());
    otherPlayerDataResponse.mutable_other_data()->CopyFrom(*self_data);
    room->Broadcast(self_player,  otherPlayerDataResponse);

    YLOG_INFO("玩家<{}>进入场景", self_data->account_data().uid())
}

void GameService::OnLeaveScene(const UserConnectionPtr& leave_conn, const Ptr<protocol::app::C2SLeaveScene> & req) //NOLINT
{
    if(leave_conn == nullptr) return;
    // 给其他玩家客户端发送离线通告
    S2CLeaveScene playerLeave;
    playerLeave.set_uid(leave_conn->GetUID());
    const auto room = m_roomManager->FindRoomByRoomID(req->room_id());
    room->Broadcast(req->uid(), playerLeave);
    YLOG_INFO("玩家<{}>离开", playerLeave.uid())

    leave_conn->SetState(UserConnection::E_UserBaseState::eSavingData);

    /* * 清除数据 and 保存数据 */
    // 删除玩家并将玩家数据归还对象池
    {
        const auto removed_player = room->RemovePlayer(leave_conn->GetUID());
        removed_player->GetBaseData()->Clear();
    }

    leave_conn->SetState(UserConnection::E_UserBaseState::eFree);
    m_frontend_server.DelUser(leave_conn->GetConnID());
    YLOG_INFO("玩家<{}>离开并保存数据！", leave_conn->GetUID());
}

/// 当userdata_self收到其他人的移动的数据时，便会申请获取id为id_other的用户的玩家数据
void GameService::OnC2SOtherPlayerData(const UserConnectionPtr& self_conn, const Ptr<protocol::app::C2SOtherPlayerData> & req) //NOLINT
{
    const auto player_other = m_roomManager->FindPlayer(req->room_id(), req->requested_uid());
    if(player_other == nullptr) {
        YLOG_WARN("Get playerdata<{}> not found", req->requested_uid())
        return;
    }
    S2COtherPlayerData response;
    response.set_uid(req->requester_uid());
    response.set_room_id(req->room_id());
    response.mutable_other_data()->CopyFrom(*player_other->GetBaseData());
    self_conn->SendTCP(response);
}

void GameService::OnC2SMove(const UserConnectionPtr& self_conn, const Ptr<C2SMove> & selfmove)
{
    const auto player_self = m_roomManager->FindPlayer(selfmove->room_id(), selfmove->uid());
    if(player_self == nullptr) {
        YLOG_WARN("Get playerdata<{}> not found", selfmove->uid())
        return;
    }

    player_self->GetBaseData()->mutable_movement()->CopyFrom(selfmove->movement());
    self_conn->SendTCP(selfmove);

    S2CMove to_other_move;
    to_other_move.set_uid(selfmove->uid());
    to_other_move.set_room_id(selfmove->room_id());
    to_other_move.mutable_movement()->CopyFrom(selfmove->movement());

    const auto room = m_roomManager->FindRoomByRoomID(selfmove->room_id());
    room->Broadcast(player_self, to_other_move);
}

void GameService::OnC2SJumpAndGravity(const UserConnectionPtr& self_conn, const Ptr<protocol::app::C2SJumpAndGravity> & selfJumpAndGravity) //NOLINT
{
    const auto player_self = m_roomManager->FindPlayer(selfJumpAndGravity->room_id(), selfJumpAndGravity->uid());
    if(player_self == nullptr) {
        YLOG_WARN("Jump playerdata<{}> not found", selfJumpAndGravity->uid())
        return;
    }

    // 记录玩家状态
    player_self->GetBaseData()->mutable_jump_and_gravity()->CopyFrom(selfJumpAndGravity->jump_and_gravity());
    self_conn->SendTCP(selfJumpAndGravity);

    S2CJumpAndGravity otherJumpAndGravity;
    otherJumpAndGravity.set_uid(selfJumpAndGravity->uid());
    otherJumpAndGravity.set_room_id(selfJumpAndGravity->room_id());
    otherJumpAndGravity.mutable_jump_and_gravity()->CopyFrom(selfJumpAndGravity->jump_and_gravity());

    const auto room = m_roomManager->FindRoomByRoomID(selfJumpAndGravity->room_id());
    room->Broadcast(player_self, otherJumpAndGravity);
}


} //namespace yy::app

