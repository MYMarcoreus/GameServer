#include "UserBaseData.h"
#include "ConfigManager.h"
#include "AppXmlConfig.h"

namespace yy::core {


UserBaseData::UserBaseData() :
        send_buf{config::g_app_config->GetValue().send_bytes_max()},
        recv_buf{config::g_app_config->GetValue().recv_bytes_max()},
        temp_recvBuf{new char[config::g_app_config->GetValue().recv_bytes_one()]},
        sock{ util::Socket::kInvalidFD },
        state {E_ServerSocketState::eFree},
        is_shutdown{false}
{
    Reset();
}

void UserBaseData::Reset()
{
    sock = util::Socket{util::Socket::kInvalidFD};
    state = E_ServerSocketState::eFree;
    is_shutdown = false;

    xorCode = config::g_app_config->GetValue().app_xor_code();
    appID   = config::g_app_config->GetValue().app_id();

    send_buf.Reset();
    send_buf.set_isCompleted(true);  // 数据是否发送完毕
    recv_buf.Reset();
    recv_buf.set_isCompleted(false); // 数据是否接受完毕
    package_len = 0;

    time_connect  = time(nullptr);
    time_heart    = time(nullptr);
    time_shutdown = time(nullptr);
}


void UserBaseData::Init(util::Socket sockboj)
{
    this->Reset();

    this->sock = sockboj;
    this->state = E_ServerSocketState::eConnected;
}


}
