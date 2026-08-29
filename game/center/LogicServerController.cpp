#include "LogicServerController.h"

#include "EventLoop.h"
#include "IPAddress.h"
#include "RoomInfoController.h"
#include "LogicInfoController.h"
#include "inner_room.pb.h"


namespace yy::app::center
{
LogicServerController::LogicServerController(const std::string& logic_service_name, net::EventLoop * loop) :
    logic_info_controller_{logic_service_name},
    room_info_controller_{},
    logic_client_(rpc_client::LogicRpcClient::Instance()),
    base_loop_{loop}
{
}

void LogicServerController::Init()
{
    // 心跳检测逻辑服
    base_loop_->RunEvery(1s, [this] { UpdateLogicInfo(); });
}

void LogicServerController::UpdateLogicInfo()
{
    // 根据逻辑服内部地址获取逻辑服外部地址
    for (auto& [name, inner_addr] : logic_client_.GetServerNames())
    {
        protocol::app::GetLogicAddrReq req;
        logic_client_.CallRemote_From<protocol::app::GetLogicAddrReq, protocol::app::GetLogicAddrRsp>(name, req)
            .then([this, name](core::rpc::RpcResult<protocol::app::GetLogicAddrRsp> result) {
                if (!result.ok()) return;
                const net::IPAddressPtr outter_addr = std::make_shared<net::IPv4Address>(result.response->ip(), result.response->port());
                logic_info_controller_.AddServerInfo(name, outter_addr);
            });
    }
}


auto LogicServerController::SelectLogicServer() -> LogicServerInfoPtr
{
    return logic_info_controller_.GetMinPlayerServerInfo();
}

auto LogicServerController::FindServerNameByRoomID(const ROOM_ID_t room_id) -> std::optional<std::string>
{
    const auto room_info = room_info_controller_.FindRoomByRoomID(room_id);
    return room_info ? std::make_optional(room_info->get_server_info()->get_name()) : std::nullopt;
}
}
