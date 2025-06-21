#include "AppXmlConfig.h"

namespace yy::config
{

//! 調用來源：parse_all_xml_nodes
// 职责分离：XmlElementTo<AppXmlConfig>负责解析，AppXmlConfig负责存储
template<>
class XmlElementTo<AppXmlConfig> {
public:
    AppXmlConfig operator()(const XMLElement * xml_app) {
        AppXmlConfig appXmlConfig;

        appXmlConfig.appID = XmlAttributeTo<uint32_t>(xml_app->FindAttribute("appID"));
        appXmlConfig.appTcpPort = XmlAttributeTo<uint16_t>(xml_app->FindAttribute("appTcpPort"));
        appXmlConfig.appUdpPort = XmlAttributeTo<uint16_t>(xml_app->FindAttribute("appUdpPort"));
        appXmlConfig.appMaxPlayer = XmlAttributeTo<int32_t>(xml_app->FindAttribute("appMaxPlayer"));
        appXmlConfig.appMaxConnection = XmlAttributeTo<int32_t>(xml_app->FindAttribute("appMaxConnection"));
        appXmlConfig.appXorCode = XmlAttributeTo<uint8_t>(xml_app->FindAttribute("appXorCode"));
        appXmlConfig.appVersion = XmlAttributeTo<uint32_t>(xml_app->FindAttribute("appVersion"));
        appXmlConfig.recvBytesOne = XmlAttributeTo<size_t>(xml_app->FindAttribute("recvBytesOne")) * 1024;
        appXmlConfig.recvBytesMax = XmlAttributeTo<size_t>(xml_app->FindAttribute("recvBytesMax")) * 1024;
        appXmlConfig.sendBytesOne = XmlAttributeTo<size_t>(xml_app->FindAttribute("sendBytesOne")) * 1024;
        appXmlConfig.sendBytesMax = XmlAttributeTo<size_t>(xml_app->FindAttribute("sendBytesMax")) * 1024;
        appXmlConfig.maxHeartTime = XmlAttributeTo<int32_t>(xml_app->FindAttribute("maxHeartTime"));
        appXmlConfig.maxSecurityTime = XmlAttributeTo<int32_t>(xml_app->FindAttribute("maxSecurityTime"));
        appXmlConfig.closeDelay = XmlAttributeTo<int32_t>(xml_app->FindAttribute("closeDelay"));
        appXmlConfig.tcpIOThreadNum = XmlAttributeTo<uint32_t>(xml_app->FindAttribute("tcpIOThreadNum"));
        appXmlConfig.udpIOThreadNum = XmlAttributeTo<uint32_t>(xml_app->FindAttribute("udpIOThreadNum"));
        appXmlConfig.workThreadNum  = XmlAttributeTo<uint32_t>(xml_app->FindAttribute("workThreadNum"));
        appXmlConfig.rpcPort  = XmlAttributeTo<uint16_t>(xml_app->FindAttribute("rpcPort"));

        auto securityCode = XmlAttributeTo<std::string>(xml_app->FindAttribute("securityCode"));
        auto checkCode    = XmlAttributeTo<std::string>(xml_app->FindAttribute("checkCode"));

        memcpy(appXmlConfig.securityCode, securityCode.c_str(), 20);
        memcpy(appXmlConfig.checkCode, checkCode.c_str(), 2);

        return appXmlConfig;
    }
};


ConfigVar<AppXmlConfig>::ptr g_app_config = ConfigManager::LookUpOrAdd<AppXmlConfig>("root.app", {});


}












