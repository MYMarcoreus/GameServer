#include "AppXmlConfig.h"

namespace yy::config
{

template<>
class XmlElementTo<AppXmlConfig> {
public:
    AppXmlConfig operator()(const XMLElement * xml_app) {
        AppXmlConfig appXmlConfig;
        appXmlConfig.load(xml_app);
        return appXmlConfig;
    }
};

void AppXmlConfig::load(const XMLElement *xml_app)
{
    appID            = XmlAttributeTo<uint32_t>(xml_app->FindAttribute("appID"));
    appPort          = XmlAttributeTo<uint16_t>(xml_app->FindAttribute("appPort"));
    appMaxPlayer     = XmlAttributeTo<int32_t>(xml_app->FindAttribute("appMaxPlayer"));
    appMaxConnection = XmlAttributeTo<int32_t>(xml_app->FindAttribute("appMaxConnection"));
    appXorCode       = XmlAttributeTo<uint8_t>(xml_app->FindAttribute("appXorCode"));
    appVersion       = XmlAttributeTo<uint32_t>(xml_app->FindAttribute("appVersion"));
    recvBytesOne     = XmlAttributeTo<size_t>(xml_app->FindAttribute("recvBytesOne")) * 1024;
    recvBytesMax     = XmlAttributeTo<size_t>(xml_app->FindAttribute("recvBytesMax")) * 1024;
    sendBytesOne     = XmlAttributeTo<size_t>(xml_app->FindAttribute("sendBytesOne")) * 1024;
    sendBytesMax     = XmlAttributeTo<size_t>(xml_app->FindAttribute("sendBytesMax")) * 1024;
    maxHeartTime     = XmlAttributeTo<int32_t>(xml_app->FindAttribute("maxHeartTime"));
    maxSecurityTime  = XmlAttributeTo<int32_t>(xml_app->FindAttribute("maxSecurityTime"));
    closeDelay       = XmlAttributeTo<int32_t>(xml_app->FindAttribute("closeDelay"));
    ioThreadNum      = XmlAttributeTo<uint32_t>(xml_app->FindAttribute("ioThreadNum"));

    memcpy(securityCode, XmlAttributeTo<std::string>(xml_app->FindAttribute("securityCode")).c_str(), 20);
    memcpy(checkCode   , XmlAttributeTo<std::string>(xml_app->FindAttribute("checkCode")).c_str(), 3);
}


ConfigVar<AppXmlConfig>::ptr g_app_config
        = ConfigManager::LookUpOrAdd<AppXmlConfig>("root.app", {});


}












