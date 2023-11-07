#include "RemoteXmlConfig.h"



namespace yy::config {


template<>
class XmlElementTo<RemoteXmlConfig::RemoteNode>
{
public:
    RemoteXmlConfig::RemoteNode operator() (const XMLElement * xml_remote_node)
    {
        return {
            XmlAttributeTo<int32_t    >(xml_remote_node->FindAttribute( "userID")),
            XmlAttributeTo<std::string>(xml_remote_node->FindAttribute( "ip")),
            XmlAttributeTo<uint16_t   >(xml_remote_node->FindAttribute( "port")),
            XmlAttributeTo<std::string>(xml_remote_node->FindAttribute( "type"))
        };
    }
};




std::string RemoteXmlConfig::RemoteNode::TypeToString() const
{
    switch(m_type) {
        case PLAYER : return "player";
        case DB     : return "db";
        case CENTER : return "center";
        case GAME   : return "game";
        case GATE   : return "gate";
        case LOGIN  : return "login";
        default     : return "unknown";
    }
}

RemoteXmlConfig::RemoteType RemoteXmlConfig::RemoteNode::StringToType(const std::string& t)
{
    std::string str{t};
    std::transform(str.begin(), str.end(), str.begin(), tolower);

    if(str == "player") return PLAYER;
    if(str == "db") return DB;
    if(str == "center") return CENTER;
    if(str == "game") return GAME;
    if(str == "gate") return GATE;
    if(str == "login") return LOGIN;

    return UNKNOWN;
}

 template<>
 class XmlElementTo<RemoteXmlConfig>
 {
 public:
     RemoteXmlConfig operator()(const XMLElement *xml_remote)
     {
         RemoteXmlConfig remoteXmlConfig;
         remoteXmlConfig.load(xml_remote);
         return remoteXmlConfig;
     }
 };


void RemoteXmlConfig::load(const XMLElement *xml_remote)
{
    appXorCode      = XmlAttributeTo<uint8_t>(xml_remote->FindAttribute("appXorCode"));
    appVersion      = XmlAttributeTo<int32_t>(xml_remote->FindAttribute("appVersion"));
    recvBytesOne    = XmlAttributeTo<int32_t>(xml_remote->FindAttribute("recvBytesOne")) * 1024;
    recvBytesMax    = XmlAttributeTo<int32_t>(xml_remote->FindAttribute("recvBytesMax")) * 1024;
    sendBytesOne    = XmlAttributeTo<int32_t>(xml_remote->FindAttribute("sendBytesOne")) * 1024;
    sendBytesMax    = XmlAttributeTo<int32_t>(xml_remote->FindAttribute("sendBytesMax")) * 1024;
    maxHeartTime    = XmlAttributeTo<int32_t>(xml_remote->FindAttribute("maxHeartTime"));
    autoConnectTime = XmlAttributeTo<int32_t>(xml_remote->FindAttribute("autoConnectTime"));
    memcpy(securityCode, XmlAttributeTo<std::string>(xml_remote->FindAttribute( "securityCode")).c_str(), 20);
    memcpy(checkCode, XmlAttributeTo<std::string>(xml_remote->FindAttribute( "checkCode")).c_str(), 3);

    m_remote_nodes = XmlElementTo<decltype(m_remote_nodes)>{}(xml_remote);
}


ConfigVar<RemoteXmlConfig>::ptr g_remote_config
        = ConfigManager::LookUpOrAdd<RemoteXmlConfig>("root.remote", {});












}
