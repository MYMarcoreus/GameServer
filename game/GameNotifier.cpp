#include "GameNotifier.h"
#include "GameTestManager.h"
#include "GamePlayerManager.h"
#include "GameProtocol.h"
#include "log.h"

#pragma clang diagnostic push
#pragma ide diagnostic ignored "performance-unnecessary-value-param"

using namespace yy::core;
using namespace yy::util;

namespace yy::app{

void AppNotifier_Connect(const UserBaseData::ptr & userdata, int32_t ) {
    // YLOG_INFO("用户<%d>已连接，等待安全验证", userdata->sock.get_fd())
}

void AppNotifier_Secutiry(const UserBaseData::ptr& userdata, int32_t result_code) {
    // YLOG_TRACE("in AppNotifier_Secutiry")
    // YLOG_INFO("用户<%d>安全验证通过：%d", userdata->sock.get_fd(), result_code)
}

void AppNotifier_Disconnect(const UserBaseData::ptr& userdata, int32_t) {
    YLOG_INFO("用户<%d>断开连接", userdata->sock.get_fd())

    // 已登陆，保存数据
    if(userdata->isLoggedIn())
    {
        YLOG_INFO("<%d> NeedSave", userdata->sock.get_fd())
        userdata->state = E_ServerSocketState::eNeedSave;
    }
    else // 未登录，重置数据
    {
        YLOG_INFO("<%d> DataReset", userdata->sock.get_fd())
        get_server_instance().setUserFree(userdata);
    }
}

//! 服务器底层解析数据包，若发现是用户层的指令，则调用该回调函数来执行指令
void AppNotifier_Command(const UserBaseData::ptr& userdata, int32_t cmd) {
    YLOG_TRACE("in AppNotifier_Command")
    assert(userdata != nullptr);

    auto command = static_cast<E_PackageCommand>(cmd);

    switch(command)
    {
        case E_PackageCommand::eLogin:
        case E_PackageCommand::eMove:
        case E_PackageCommand::eGetPlayerData:
        case E_PackageCommand::eLeave:
        case E_PackageCommand::eJumpAndGravity:GamePlayerManager::getInstance().AppCommand(userdata, cmd);
            break;
        case E_PackageCommand::eTest:GameTestManager::getInstance().AppCommand(userdata, cmd);
            break;
        default:
            YLOG_WARN("Unsupported Package Command<%04x>!", cmd)
            break;
    }
}

}
#pragma clang diagnostic pop
