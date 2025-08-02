#pragma once

#include "core_definations.h"
#include "msg_cmd.pb.h"
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>

#include "account.pb.h"
#include "connection.pb.h"
#include "room.pb.h"
#include "game.pb.h"
#include "inner_room.pb.h"

using namespace yy::protocol;
using namespace yy::protocol::app;
using namespace yy::protocol::core;

namespace yy::core
{
inline std::unordered_map<MessageCommand, std::string> g_cmd_to_name = {
    {MSG_Unknown, ""},

    // 连接相关
    {MSG_HeartBody, HeartBody::descriptor()->full_name()},
    {MSG_XorBodyRsp, XorBodyRsp::descriptor()->full_name()},
    {MSG_SecurityCheckReq, SecurityCheckReq::descriptor()->full_name()},
    {MSG_SecurityCheckRsp, SecurityCheckRsp::descriptor()->full_name()},
    {MSG_UdpPortRegisterReq, UdpPortRegisterReq::descriptor()->full_name()},
    {MSG_UdpPortRegisterRsp, UdpPortRegisterRsp::descriptor()->full_name()},

    // 登录模块：网关服->账号服
    {MSG_LoginReq, LoginReq::descriptor()->full_name()},
    {MSG_LoginRsp, LoginRsp::descriptor()->full_name()},
    {MSG_RegisterReq, RegisterReq::descriptor()->full_name()},
    {MSG_RegisterRsp, RegisterRsp::descriptor()->full_name()},

    // 房间模块：网关服->中心服
    {MSG_CreateRoomReq, CreateRoomReq::descriptor()->full_name()},
    {MSG_CreateRoomRsp, CreateRoomRsp::descriptor()->full_name()},
    {MSG_SearchRoomReq, SearchRoomReq::descriptor()->full_name()},
    {MSG_SearchRoomRsp, SearchRoomRsp::descriptor()->full_name()},
    {MSG_JoinRoomReq, JoinRoomReq::descriptor()->full_name()},
    {MSG_JoinRoomRsp, JoinRoomRsp::descriptor()->full_name()},
    {MSG_QuitRoomReq, QuitRoomReq::descriptor()->full_name()},
    {MSG_QuitRoomRsp, QuitRoomRsp::descriptor()->full_name()},
    {MSG_GetEnterSceneTokenReq, GetEnterSceneTokenReq::descriptor()->full_name()},
    {MSG_GetEnterSceneTokenRsp, GetEnterSceneTokenRsp::descriptor()->full_name()},
    // 房间广播：中心服->网关服
    {MSG_BroadcastRoomReq, BroadcastRoomReq::descriptor()->full_name()},
    {MSG_BroadcastRoomRsp, BroadcastRoomRsp::descriptor()->full_name()},

    // 房间增删：中心服->逻辑服
    {MSG_NewRoomReq, NewRoomReq::descriptor()->full_name()},
    {MSG_NewRoomRsp, NewRoomRsp::descriptor()->full_name()},
    {MSG_DeleteRoomReq, DeleteRoomReq::descriptor()->full_name()},
    {MSG_DeleteRoomRsp, DeleteRoomRsp::descriptor()->full_name()},

    // 场景模块：逻辑服->客户端
    {MSG_SceneLoginReq, SceneLoginReq::descriptor()->full_name()},
    {MSG_SceneLoginRsp, SceneLoginRsp::descriptor()->full_name()},
    {MSG_C2SEnterScene, C2SEnterScene::descriptor()->full_name()},
    {MSG_S2CEnterScene, S2CEnterScene::descriptor()->full_name()},
    {MSG_C2SLeaveScene, C2SLeaveScene::descriptor()->full_name()},
    {MSG_S2CLeaveScene, S2CLeaveScene::descriptor()->full_name()},
    {MSG_C2SMove, C2SMove::descriptor()->full_name()},
    {MSG_S2CMove, S2CMove::descriptor()->full_name()},
    {MSG_C2SJumpAndGravity, C2SJumpAndGravity::descriptor()->full_name()},
    {MSG_S2CJumpAndGravity, S2CJumpAndGravity::descriptor()->full_name()},
    {MSG_C2SOtherPlayerData, C2SOtherPlayerData::descriptor()->full_name()},
    {MSG_S2COtherPlayerData, S2COtherPlayerData::descriptor()->full_name()},
};

inline std::unordered_map<std::string, MessageCommand> g_name_to_cmd = [] {
    std::unordered_map<std::string, MessageCommand> map;
    for (const auto& [cmd, name] : g_cmd_to_name) {
        if (!name.empty()) {
            map[name] = cmd;
        }
    }
    return map;
}();

inline MessagePtr CreateMessage(const std::string &typeName) {
    using namespace google::protobuf;

    MessagePtr message = nullptr;
    const Descriptor * des = DescriptorPool::generated_pool()->FindMessageTypeByName(typeName);
    if(des)
    {
        const Message * prototype = MessageFactory::generated_factory()->GetPrototype(des);
        if(prototype) {
            message.reset(prototype->New());
        }
    }
    return message;
}

inline MessagePtr CreateMessage(const MessageCommand msg_cmd) {
    const std::string & msg_name = g_cmd_to_name[msg_cmd];
    return CreateMessage(msg_name);
}

}