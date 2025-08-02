#include "LogicServerController.h"

#include "IPAddress.h"
#include "RoomInfoController.h"
#include "LogicInfoController.h"


namespace yy::app::center
{
LogicServerController::LogicServerController(const std::string& logic_service_name) :
    logic_info_controller_{logic_service_name},
    room_info_controller_{},
    logic_client_(rpc_client::LogicRpcClient::Instance())
{

}

void LogicServerController::Init()
{
    // 获取逻辑服地址并保存
    const auto server_names = logic_client_.GetServerNames();
    for (const auto& [name, addr] : server_names) {
        const bool success = logic_info_controller_.AddServerInfo(addr->GetIPStr(), addr->GetPort(), name);
        if (not success) {
            YLOG_WARN("Logic server {} already exists", name);
        }
    }
    (void)0;
}


auto LogicServerController::SelectLogicServer() -> LogicServerInfoPtr
{
    return logic_info_controller_.GetMinPlayerServerInfo();
}

auto LogicServerController::FindServerNameByRoomID(const ROOM_ID_t room_id) -> std::optional<std::string>
{
    const auto room_info = room_info_controller_.FindRoomByRoomID(room_id);
    return room_info ? std::make_optional(room_info->server_name) : std::nullopt;
}
}
