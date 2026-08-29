#include "GameService.h"
#include "EventLoopThreadPool.h"
#include "LogicServerManager.h"
#include "log.h"
#include "UserConnection.h"
#include "IServer.h"
#include "RoomManager.h"
#include "EventLoop.h"
#include "LogicRedisDAO.h"
#include "AppXmlConfig.h"


using namespace yy::net;
using namespace yy::core;
using namespace yy::util;
using namespace yy::protocol::app;

namespace yy::app::logic {
GameService::GameService(EventLoop * baseLoop, IServer& frontend):
    m_baseLoop{baseLoop},
    m_frontend{frontend},
    m_room_dispatcher{[this](const UserConnectionPtr& conn, const MessagePtr& msg)
    {
        if (!conn) {
            YLOG_ERROR("GameService Dispatcher：连接为空，消息{}", msg->DebugString())
            return;
        }
        // YLOG_INFO("User {} Dispatching msg: {}", conn->GetUID(), msg->ShortDebugString())

        if (auto room = m_roomManager->FindRoomByUID(conn->GetUID())) {
            //! 即时处理（非Update）：对于游戏消息，并不在IO线程处理，而是在专门处理游戏数据的工作线程中处理
            room.Send([conn, msg](Room& r) {
                r.PostMessage(conn, msg);
            });
        }
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

    ObjectPool<PlayerBaseData>::Init("玩家对象池", m_frontend.GetAppConfig().app_player_max(), nullptr, nullptr);
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
    if (auto room = m_roomManager->FindRoomByUID(userconn->GetUID())) {
        room.Send([userconn](Room& r) {
            r.OnPlayerDisconnect(userconn);
        });
    }
}

void GameService::NewRoom(google::protobuf::RpcController* controller,
    const NewRoomReq* request, NewRoomRsp* response, google::protobuf::Closure* done)
{
    YLOG_INFO("正在执行 GameService::NewRoom 服务: {}", request->ShortDebugString())
    //! 新建房间但是不新建Player，需要在客户端连接logic server时再AddPlayer
    const auto new_room = m_roomManager->AddRoom(request->room_data());
    response->set_success(new_room.IsAlive());
    response->set_room_id(request->room_data().room_id());
    // 给出逻辑服对外开放的ip和端口
    const auto fronend_addr = m_frontend.GetTcpListenAddr();
    // 优先返回配置中的 advertiseIp，若为空则回落到本机IP
    const auto & advertise = yy::config::g_app_config->GetValue().advertise_ip();
    if (!advertise.empty()) response->set_ip(advertise);
    else response->set_ip(GetLocalIP());
    response->set_port(fronend_addr->GetPort());
    YLOG_INFO("执行完毕 GameService::NewRoom 服务: {}", response->ShortDebugString())

    // 发送响应
    done->Run();
}

void GameService::DeleteRoom(google::protobuf::RpcController* controller,
    const DeleteRoomReq* request, DeleteRoomRsp* response, google::protobuf::Closure* done)
{
    YLOG_INFO("正在执行 GameService::DeleteRoom 服务: {}", request->ShortDebugString())
    response->set_room_id(request->room_id());

    // 删除房间
    response->set_success(m_roomManager->RemoveRoom(request->room_id()));
    YLOG_INFO("执行完毕 GameService::DeleteRoom 服务: {}", response->ShortDebugString())

    // 发送响应
    done->Run();
}

void GameService::GetLogicAddr(google::protobuf::RpcController* controller,
    const GetLogicAddrReq* request, GetLogicAddrRsp* response, google::protobuf::Closure* done)
{
    // 获取逻辑服对游戏客户端开放的地址
    const auto frontend_addr = m_frontend.GetTcpListenAddr();
    const auto & advertise = yy::config::g_app_config->GetValue().advertise_ip();

    if (!advertise.empty()) {
        response->set_ip(advertise);
    }
    else {
        response->set_ip(GetLocalIP());
    }
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
    const auto io_loop = conn->GetConnection()->GetIOLoop(); //! 记录 IO 线程，稍后投回

    //! Redis 阻塞调用挪到工作线程，避免卡住 IO 线程
    m_redisDAO.FetchLoginDataAsync(uid)
        .then([this, conn, req, uid, io_loop](LoginData data) -> bool {
            // 本续体在 Redis 工作线程执行；连接相关操作投回 IO 线程
            io_loop->RunCallbackInLoop([this, conn, req, uid, data = std::move(data)]() mutable {
                CompleteLogin(conn, req, uid, std::move(data));
            });
            return true;
        })
        .onError([](std::exception_ptr) -> bool {
            YLOG_ERROR("登录失败：Redis 查询异常");
            return true;
        });
}

void GameService::CompleteLogin(const UserConnectionPtr& conn, const Ptr<SceneLoginReq>& req, const UID_t uid, LoginData data)
{
    SceneLoginRsp resp;
    resp.set_is_ok(false);

    const std::string scene_token_req = req->scene_token();
    const std::string user_token_req  = req->user_token();

    if (!data.scene_token || !data.user_token) {
        YLOG_ERROR("登录失败：Redis中找不到token [uid={}]", uid);
        conn->SendTCP(resp);
        return;
    }

    if (scene_token_req != *data.scene_token || user_token_req != *data.user_token) {
        YLOG_ERROR("登录失败：token校验失败 [uid={}]", uid);
        conn->SendTCP(resp);
        return;
    }

    if (!data.account) {
        YLOG_ERROR("获取redis账号数据失败 [uid={}]", uid);
        conn->SendTCP(resp);
        return;
    }

    // 验证通过，登录成功
    conn->SetUID(uid);
    conn->SetToken(user_token_req);
    conn->SetState(UserConnection::E_UserBaseState::eLoggedIn);

    resp.set_is_ok(true);
    conn->SendTCP(resp);

    AccountBaseData account_data;
    account_data.set_uid(data.account->uid);
    account_data.set_username(data.account->username);

    m_roomManager->AddPlayerToRoom(req->room_id(), std::move(account_data), conn)
        .then([uid](bool ok) -> bool {
            if (ok) {
                YLOG_INFO("[GameService::OnSceneLoginReq] UID {} 验证成功，成功加入房间", uid);
            } else {
                YLOG_INFO("[GameService::OnSceneLoginReq] UID {} 验证成功，但加入房间失败", uid);
            }
            return true;
        });
}


} //namespace yy::app
