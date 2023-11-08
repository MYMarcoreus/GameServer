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



void AppNotifier_Secutiry(const yy::net::TcpConnectionPtr& conn, int32_t result_code) {
    // YLOG_TRACE("in AppNotifier_Secutiry")
    // YLOG_INFO("用户<%d>安全验证通过：%d", userdata->sock.get_fd(), result_code)
}

void AppNotifier_Disconnect(const yy::net::TcpConnectionPtr& conn, int32_t) {
    YLOG_INFO("用户<{}>断开连接", conn->GetSocketFD())

    auto & userdata = get_server_instance().FindUser(conn);

    // 已登陆，保存数据
    if(userdata->isLoggedIn())
    {
        YLOG_INFO("<{}> NeedSave", conn->GetSocketFD())
        get_server_instance().FindUser(conn)->SetState(core::UserBaseData::E_UserBaseState::eNeedSave);
    }
    else // 未登录，重置数据
    {
        YLOG_INFO("<{}> DataReset", conn->GetSocketFD())
        get_server_instance().setUserFree(userdata);
    }
}


}
#pragma clang diagnostic pop
