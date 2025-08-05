#include "GameService.h"
#include "EventLoopThreadPool.h"
#include "LogicServerManager.h"
#include "log.h"
#include "UserConnection.h"
#include "IServer.h"
#include "RoomManager.h"
#include "EventLoop.h"
#include "LogicRedisDAO.h"


using namespace yy::net;
using namespace yy::core;
using namespace yy::util;
using namespace yy::protocol::app;

namespace yy::app::logic {
GameService::GameService(EventLoop * baseLoop, IServer& frontend):
    m_baseLoop{baseLoop},
    m_frontend{frontend},
    m_room_dispatcher{[this](const UserConnectionPtr& conn, const MessagePtr& msg) {
        m_roomManager->FindRoomByUID(conn->GetUID(),
            [conn, msg](const RoomPtr & room) {
                // YLOG_INFO("收到消息{}: {}", msg->GetDescriptor()->name(), msg->ShortDebugString())
                if (room) {
                    //! 即时处理（非Update）：对于游戏消息，并不在IO线程处理，而是在专门处理游戏数据的工作线程中处理（让分发器找到该游戏消息所注册的对应的处理函数。）
                    room->PostMessage(conn, msg);
                }
            });
    }},
    m_players_pool{ObjectPool<PlayerBaseData>::Instance()},
    m_redisDAO{LogicRedisDAO::Instance()}
{
    m_frontend.SetNotifier_Command(
        [this](const UserConnectionPtr & userdata, const MessagePtr & message, const MessageNetType type) {
            this->DispatchMessage(userdata, message);
        });

    m_frontend.SetNotifier_DisConnect(
        [this](const UserConnectionPtr & userdata) {
            this->OnPlayerDisconnect(userdata);
        });

    RegisterHandler(this, this->m_room_dispatcher, &GameService::OnSceneLoginReq);

    m_players_pool.Init("玩家对象池", m_frontend.GetAppConfig().app_player_max(), nullptr, nullptr);
    m_roomManager = std::make_unique<RoomManager>(m_baseLoop);
    m_redisDAO.Start(m_baseLoop);
}

GameService::~GameService() = default;

void GameService::DispatchMessage(const UserConnectionPtr& conn, const MessagePtr& msg) const
{
    m_room_dispatcher.OnProtobufMessage(conn, msg);
}

void GameService::OnPlayerDisconnect(const UserConnectionPtr& userconn)
{
    if(!userconn or !userconn->IsLoggedIn()) return;
    m_roomManager->FindRoomByUID(userconn->GetUID(),
        [userconn](const RoomPtr & room) {
            if (room) {
                room->OnPlayerDisconnect(userconn);
            }
        });
}

void GameService::NewRoom(google::protobuf::RpcController* controller, const NewRoomReq* request, NewRoomRsp* response,
                          google::protobuf::Closure* done)
{
    YLOG_INFO("正在执行 GameService::NewRoom 服务: {}", request->ShortDebugString())
    // 需要在新建房间时为房间分配线程，分配器需要保证在其所在的线程中进行分配，所以使用异步
    m_roomManager->AddRoom(request->room_data(),
        [this, response, done] (const RoomPtr& new_room) {
            //! 新建房间但是不新建Player，需要在客户端连接logic server时再AddPlayer
            response->set_success(true);
            response->set_room_id(new_room->get_id());
            // 给出逻辑服对外开放的ip和端口
            const auto fronend_addr = m_frontend.GetTcpListenAddr();
            response->set_ip(fronend_addr->GetIPStr());
            response->set_port(fronend_addr->GetPort());
            YLOG_INFO("执行完毕 GameService::NewRoom 服务: {}", response->ShortDebugString())

            // 发送响应
            done->Run();
        });
}

void GameService::DeleteRoom(google::protobuf::RpcController* controller, const DeleteRoomReq* request,
    DeleteRoomRsp* response, google::protobuf::Closure* done)
{
    YLOG_INFO("正在执行 GameService::DeleteRoom 服务: {}", request->ShortDebugString())
    response->set_room_id(request->room_id());

    // 删除房间
    m_roomManager->RemoveRoom(request->room_id(),
        [response, done] (const bool is_removed) {
            // 填写响应
            response->set_success(is_removed);
            // 发送响应
            done->Run();
            YLOG_INFO("执行完毕 GameService::DeleteRoom 服务: {}", response->ShortDebugString())
        });
}

void GameService::GetLogicAddr(google::protobuf::RpcController* controller,
    const GetLogicAddrReq* request, GetLogicAddrRsp* response, google::protobuf::Closure* done)
{
    const auto frontend_addr = m_frontend.GetTcpListenAddr();
    response->set_ip(frontend_addr->GetIPStr());
    response->set_port(frontend_addr->GetPort());

    // 发送响应
    done->Run();
}

void GameService::OnSceneLoginReq(const UserConnectionPtr& conn, const Ptr<SceneLoginReq>& req)
{
    SceneLoginRsp resp;
    resp.set_is_ok(false);

    if (conn->IsLoggedIn()) {
        YLOG_WARN("连接已经登录逻辑服，重复登录请求被拒绝 [uid={}]", conn->GetUID());
        conn->SendTCP(resp);
        return;
    }

    const UID_t uid = req->uid();
    const std::string scene_token_req = req->scene_token();
    const std::string user_token_req  = req->user_token();
    const auto scene_token = m_redisDAO.GetAndDelSceneToken(uid);
    const auto user_token  = m_redisDAO.GetUserTokenAndRefreshEx(uid);

    if (!scene_token || !user_token) {
        YLOG_ERROR("登录失败：Redis中找不到token [uid={}]", uid);
        conn->SendTCP(resp);
        return;
    }

    const bool is_scene_token_ok = (scene_token_req == scene_token);
    const bool is_user_token_ok  = (user_token_req  == user_token);

    if (!is_scene_token_ok || !is_user_token_ok) {
        YLOG_ERROR("登录失败：token校验失败 [uid={} scene_ok={} user_ok={}]", uid, is_scene_token_ok, is_user_token_ok);
        conn->SendTCP(resp);
        return;
    }

    // 验证通过，登录成功
    conn->SetUID(uid);
    conn->SetToken(user_token_req);
    conn->SetState(UserConnection::E_UserBaseState::eLoggedIn);

    resp.set_is_ok(true);
    conn->SendTCP(resp);

    // 加入房间（异步）
    const ROOM_ID_t room_id = req->room_id();
    m_roomManager->FindRoomByRoomID(room_id, [this, conn, uid, room_id](const RoomPtr& room) {
        if (room) {
            m_roomManager->AddPlayerToRoom(room_id, uid, conn,
                [uid](const RoomPtr& _room) {
                    YLOG_INFO("UID {} 登录成功，加入房间 {}:{}", uid, _room->get_name(), _room->get_id());
                });
        } else {
            YLOG_ERROR("找不到房间 room_id = {}", room_id);
        }
    });
}


} //namespace yy::app

