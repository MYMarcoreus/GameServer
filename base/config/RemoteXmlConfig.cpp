#include "RemoteXmlConfig.h"



namespace yy::config {

template<>
class XmlElementTo<RemoteXmlConfig::RemoteNode>
{
public:
    RemoteXmlConfig::RemoteNode operator() (const tinyxml2::XMLElement * xml_remote_node) const
    {
        return {
            XmlAttributeTo<std::string>(xml_remote_node->FindAttribute( "username")),
            XmlAttributeTo<std::string>(xml_remote_node->FindAttribute( "password")),
            XmlAttributeTo<size_t>(xml_remote_node->FindAttribute( "poolsize")),
            XmlAttributeTo<std::string>(xml_remote_node->FindAttribute( "ip")),
            XmlAttributeTo<uint16_t>(xml_remote_node->FindAttribute( "port")),
            XmlAttributeTo<std::string>(xml_remote_node->FindAttribute( "type"))
        };
    }
};

//! ↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑
//! 調用來源：parse_all_xml_nodes
 template<>
 class XmlElementTo<RemoteXmlConfig>
 {
 public:
     RemoteXmlConfig operator()(const tinyxml2::XMLElement *xml_remote) const
     {
         RemoteXmlConfig remoteXmlConfig;

         remoteXmlConfig.appXorCode      = XmlAttributeTo<uint8_t>(xml_remote->FindAttribute("appXorCode"));
         remoteXmlConfig.appVersion      = XmlAttributeTo<int32_t>(xml_remote->FindAttribute("appVersion"));
         remoteXmlConfig.recvBytesOne    = XmlAttributeTo<int32_t>(xml_remote->FindAttribute("recvBytesOne")) * 1024;
         remoteXmlConfig.recvBytesMax    = XmlAttributeTo<int32_t>(xml_remote->FindAttribute("recvBytesMax")) * 1024;
         remoteXmlConfig.sendBytesOne    = XmlAttributeTo<int32_t>(xml_remote->FindAttribute("sendBytesOne")) * 1024;
         remoteXmlConfig.sendBytesMax    = XmlAttributeTo<int32_t>(xml_remote->FindAttribute("sendBytesMax")) * 1024;
         remoteXmlConfig.maxHeartTime    = XmlAttributeTo<int32_t>(xml_remote->FindAttribute("maxHeartTime"));
         remoteXmlConfig.autoConnectTime = XmlAttributeTo<int32_t>(xml_remote->FindAttribute("autoConnectTime"));
         memcpy(remoteXmlConfig.securityCode, XmlAttributeTo<std::string>(xml_remote->FindAttribute( "securityCode")).c_str(), 20);
         memcpy(remoteXmlConfig.checkCode, XmlAttributeTo<std::string>(xml_remote->FindAttribute( "checkCode")).c_str(), 2);

         remoteXmlConfig.m_remote_nodes = XmlElementTo<decltype(remoteXmlConfig.m_remote_nodes)>{}(xml_remote);

         return remoteXmlConfig;
     }
 };



ConfigVar<RemoteXmlConfig>::ptr g_remote_config
        = ConfigManager::LookUpOrAdd<RemoteXmlConfig>("root.remote", {});












}

