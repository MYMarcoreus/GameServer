
#ifndef ____REMOTEXMLCONFIG_H
#define ____REMOTEXMLCONFIG_H


#include "Singleton.h"
#include "ConfigManager.h"


namespace yy::config {


/// @brief 加载服务器作为客户端时的xml配置文件
struct RemoteXmlConfig
{
    enum RemoteType : int8_t
    {
        UNKNOWN,
        PLAYER, // 玩家
        DB,     // DB
        CENTER, // 中心服务器
        GAME,   // 游戏服务器
        GATE,   // 网关服务器
        LOGIN   // 登录服务器
    };


    struct RemoteNode
    {
        RemoteNode() = default;

        RemoteNode(int32_t id, std::string ip,
                   uint16_t port, const std::string& type)
            : m_id{id}, m_ip{std::move(ip)}, m_port{port}, m_type{StringToType(type)} {}

        int32_t        m_id{ };
        std::string    m_ip{ };
        uint16_t       m_port{ };
        RemoteType     m_type{ };

        [[maybe_unused]] [[nodiscard]] std::string TypeToString() const;

        static RemoteType StringToType(const std::string& t);

    };

public:
    uint8_t appXorCode{ };      // 异或码 主要用于数据加密
    int32_t appVersion{ };      // 当前应用程序版本号

    int32_t recvBytesOne{ };    // 当前一次接收数据字节长度
    int32_t recvBytesMax{ };    // 最大接收缓冲区
    int32_t sendBytesOne{ };    // 当前一次发送数据字节长度
    int32_t sendBytesMax{ };    // 最大发送缓冲区

    int32_t maxHeartTime{ };    // 心跳时间：每隔一段时间检测与客户端的连接
    int32_t autoConnectTime{ }; // 自动重连时间：用于客户端

    char securityCode[20]{ };  // md5码加密
    char checkCode[3]{ };

    std::vector<RemoteNode> m_remote_nodes;
public:

    /// @brief 读取root元素下名为remote的配置项
    void load(const XMLElement *xml_remote);
};



extern ConfigVar<RemoteXmlConfig>::ptr g_remote_config;


}


#endif //____REMOTEXMLCONFIG_H
