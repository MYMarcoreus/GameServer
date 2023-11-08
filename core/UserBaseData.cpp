#include "UserBaseData.h"
#include "ConfigManager.h"
#include "AppXmlConfig.h"

namespace yy::core {


UserBaseData::UserBaseData(net::TcpConnectionPtr conn, uint32_t appid)
        : m_conn{conn},
          m_state{E_UserBaseState::eConnected},
          m_appID(appid)
{

}







}
