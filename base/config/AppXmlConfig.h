#ifndef ____APPXMLCONFIG_H
#define ____APPXMLCONFIG_H


#include "Singleton.h"
#include "ConfigManager.h"

namespace yy::config {

/// @brief 加载「服务器端」的服务器xml配置文件
class AppXmlConfig
{
public:
    /// @brief 服务器端口号
    [[nodiscard]] uint16_t app_port() const { return appPort; }

    /// @brief 服务器ID：可用于判断服务器类型
    [[nodiscard]] uint32_t app_id() const { return appID; }

    /// @brief 最大玩家数量
    [[nodiscard]] int32_t app_player_max() const { return appMaxPlayer; }

    /// @brief 最大客户端连接数量
    [[nodiscard]] int32_t app_connection_max() const { return appMaxConnection; }

    /// @brief 初始异或码，用于首部加密
    [[nodiscard]] uint8_t app_xor_code() const { return appXorCode; }

    /// @brief 当前应用程序版本号
    [[nodiscard]] uint32_t app_version() const { return appVersion; }

    /// @brief 当前一次接收数据字节长度
    [[nodiscard]] size_t recv_bytes_one() const { return recvBytesOne; }

    /// @brief 最大接收缓冲区
    [[nodiscard]] size_t recv_bytes_max() const { return recvBytesMax; }

    /// @brief 当前一次发送数据字节长度
    [[nodiscard]] size_t send_bytes_one() const { return sendBytesOne; }

    /// @brief 最大发送缓冲区
    [[nodiscard]] size_t send_bytes_max() const { return sendBytesMax; }

    [[nodiscard]] int32_t close_delay() const { return closeDelay;  };

    /// @brief 心跳时间：每隔一段时间检测与客户端的连接
    [[nodiscard]] int32_t time_heart_max() const { return maxHeartTime; }

    /// @brief 心跳时间：每隔一段时间检测与客户端的连接
    [[nodiscard]] int32_t time_security_max() const { return maxSecurityTime; }

    /// @brief md5码加密
    [[nodiscard]] const char *security_code() const { return securityCode; }

    /// @brief 用来判断数据包是否是我们的游戏协议包的校验码
    [[nodiscard]] const char *check_code() const { return checkCode; }

    /// @brief IO线程个数
    [[nodiscard]] uint32_t io_thread_num() const { return ioThreadNum; };


    ///! @brief 读取root元素下名为app的配置项
    void load(const XMLElement *xml_app) ;

private:
    uint16_t appPort{};          // 服务器端口号
    uint32_t appID{};            // 服务器ID：可用于判断服务器类型
    int32_t  appMaxPlayer{};     // 最大玩家数量
    int32_t  appMaxConnection{}; // 最大客户端连接数量
    uint8_t  appXorCode{};       // 异或码 主要用于数据加密
    uint32_t appVersion{};       // 当前应用程序版本号

    size_t recvBytesOne{};    // 当前一次接收数据字节长度
    size_t recvBytesMax{};    // 最大接收缓冲区
    size_t sendBytesOne{};    // 当前一次发送数据字节长度
    size_t sendBytesMax{};    // 最大发送缓冲区

    int32_t maxHeartTime{};    // 心跳时间：每隔一段时间检测与客户端的连接
    int32_t maxSecurityTime{}; // 最大安全验证时间
    int32_t closeDelay{};      // 连接被shutdown后，

    char securityCode[20]{};  // md5码加密
    char checkCode[3]{};      // 游戏协议校验码

    uint32_t ioThreadNum{};
};

extern ConfigVar<AppXmlConfig>::ptr g_app_config;





}

#endif //____APPXMLCONFIG_H

