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

#define ADD_PROTO(msg) {MSG_##msg, &msg::default_instance()}


using namespace yy::protocol;
using namespace yy::protocol::app;
using namespace yy::protocol::core;

namespace yy::core
{

///@brief 消息命令与消息对象的映射
inline std::unordered_map<MessageCommand, const google::protobuf::Message*> g_cmd_to_prototype =
{
    {MSG_Unknown, nullptr},

    // 连接相关
    ADD_PROTO(HeartBody),
    ADD_PROTO(XorBodyRsp),
    ADD_PROTO(SecurityCheckReq),
    ADD_PROTO(SecurityCheckRsp),
    ADD_PROTO(UdpPortRegisterReq),
    ADD_PROTO(UdpPortRegisterRsp),

    // 登录模块：客户端->网关服->账号服
    ADD_PROTO(LoginReq),
    ADD_PROTO(LoginRsp),
    ADD_PROTO(RegisterReq),
    ADD_PROTO(RegisterRsp),
    ADD_PROTO(QuitLoginReq),
    ADD_PROTO(QuitLoginRsp),

    // 房间模块：客户端->网关服->中心服
    ADD_PROTO(CreateRoomReq),
    ADD_PROTO(CreateRoomRsp),
    ADD_PROTO(SearchRoomReq),
    ADD_PROTO(SearchRoomRsp),
    ADD_PROTO(SelfJoinRoomReq),
    ADD_PROTO(SelfJoinRoomRsp),
    ADD_PROTO(SelfQuitRoomReq),
    ADD_PROTO(SelfQuitRoomRsp),
    ADD_PROTO(OtherJoinRoomRsp),
    ADD_PROTO(OtherQuitRoomRsp),
    ADD_PROTO(GetEnterSceneTokenReq),
    ADD_PROTO(GetEnterSceneTokenRsp),
    // 房间广播：中心服->网关服
    ADD_PROTO(BroadcastRoomReq),
    ADD_PROTO(BroadcastRoomRsp),

    // 房间增删：中心服->逻辑服
    ADD_PROTO(NewRoomReq),
    ADD_PROTO(NewRoomRsp),
    ADD_PROTO(DeleteRoomReq),
    ADD_PROTO(DeleteRoomRsp),

    // 场景模块：客户端->逻辑服
    ADD_PROTO(SceneLoginReq),
    ADD_PROTO(SceneLoginRsp),
    ADD_PROTO(C2SEnterScene),
    ADD_PROTO(S2CEnterScene),
    ADD_PROTO(C2SLeaveScene),
    ADD_PROTO(S2CLeaveScene),
    ADD_PROTO(C2SMove),
    ADD_PROTO(S2CMove),
    ADD_PROTO(C2SJumpAndGravity),
    ADD_PROTO(S2CJumpAndGravity),
    ADD_PROTO(C2SOtherPlayerData),
    ADD_PROTO(S2COtherPlayerData),
};


///@brief 消息命令与消息名称的映射
inline std::unordered_map<MessageCommand, std::string> g_cmd_to_name = [] {
    std::unordered_map<MessageCommand, std::string> map;
    for (const auto& [cmd, prototype] : g_cmd_to_prototype) {
        if (prototype and prototype->GetDescriptor()) {
            map[cmd] = prototype->GetDescriptor()->full_name();
        }
    }
    return map;
}();

///@brief 消息名称与消息命令的映射
inline std::unordered_map<std::string, MessageCommand> g_name_to_cmd = [] {
    std::unordered_map<std::string, MessageCommand> map;
    for (const auto& [cmd, name] : g_cmd_to_name) {
        if (!name.empty()) {
            map[name] = cmd;
        }
    }
    return map;
}();

///@brief 根据消息名称生成消息对象
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

///@brief 根据枚举命令生成为消息对象
inline MessagePtr CreateMessage(const MessageCommand msg_cmd) {
    MessagePtr message = nullptr;
    const google::protobuf::Message* const prototype = g_cmd_to_prototype[msg_cmd];
    if (prototype) {
        message.reset(prototype->New());
    }
    return message;
}

}