#pragma once
#include <string>

#include "LogicInfoController.h"
#include "LogicRpcClient.h"
#include "RoomInfoController.h"

namespace yy::core::zk
{
class ZkServiceClient;
}

namespace yy::app::center
{

class LogicServerController {
public:
    explicit LogicServerController(const std::string& logic_service_name);

    void Init();

    auto SelectLogicServer() -> LogicServerInfoPtr;

    [[nodiscard]] LogicInfoController& get_logic_info_controller() { return logic_info_controller_; }
    [[nodiscard]] RoomInfoController& get_room_info_controller()  { return room_info_controller_; }

    ///@brief 根据房间ID获取房间所在的服务器
    auto FindServerNameByRoomID(ROOM_ID_t room_id) -> std::optional<std::string>;



private:
    LogicInfoController                         logic_info_controller_;
    RoomInfoController                          room_info_controller_;
    rpc_client::LogicRpcClient&                         logic_client_;
};

}
