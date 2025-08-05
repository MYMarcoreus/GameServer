#include "LogicServerController.h"

#include "IPAddress.h"
#include "RoomInfoController.h"
#include "LogicInfoController.h"
#include "inner_room.pb.h"


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
    auto server_names = logic_client_.GetServerNames();

    // 根据逻辑服内部地址获取逻辑服外部地址
    for (auto& [name, inner_addr] : server_names) {
        protocol::app::GetLogicAddrReq req;
        logic_client_.CallRemoteAsync_From<protocol::app::GetLogicAddrReq, protocol::app::GetLogicAddrRsp>(name,
            req,
            [this, name](std::unique_ptr<protocol::app::GetLogicAddrRsp> && response, std::unique_ptr<core::rpc::RpcControllerImpl> && controller) {
                if (response == nullptr or controller == nullptr or controller->Failed()) {
                    return;
                }
                const net::IPAddressPtr outter_addr = std::make_shared<net::IPv4Address>(response->ip(), response->port());
                logic_info_controller_.AddServerInfo(name, outter_addr);
            });
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
    return room_info ? std::make_optional(room_info->get_server_info()->get_name()) : std::nullopt;
}
}
