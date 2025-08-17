#pragma once

#include "socket_definations.h"
#include <memory>
#include <string>
#include <sys/types.h>

namespace yy::net {

class IPAddress
{
public:
    using ptr = std::shared_ptr<IPAddress>;


    IPAddress() = default;

    virtual  ~IPAddress() = default;

    /// @brief 获取协议族
    virtual sa_family_t GetFamily() const = 0;

    /// @brief 获取 socket 地址
    virtual sockaddr * const GetRawAddr() const = 0;

    /// @brief 获取地址长度
    virtual socklen_t GetRawAddrLen() const = 0;

    /// @brief 获取端口号
    virtual uint16_t GetPort() const = 0;

    virtual std::string GetPortStr() const = 0;

    /// @brief 设置端口号
    virtual void SetPort(uint16_t port) = 0;

    virtual uint32_t GetIP() const = 0;

    /// @brief 转换为字符串
    virtual std::string GetIPStr() const = 0;

    virtual std::string ToString() const = 0;
};


class IPv4Address final : public IPAddress
{
public:
    using ptr = std::shared_ptr<IPv4Address>;

    //! 构造函数
    IPv4Address();

    explicit IPv4Address(const sockaddr_in *addr) : m_address{*addr} {}

    explicit IPv4Address(const std::string & ipv4_str, uint16_t port);

    explicit IPv4Address(uint32_t ipv4, uint16_t port);

    ///@brief 只给出端口，在bind中表示监听所有本机网卡上的 IPv4 地址
    explicit IPv4Address(uint16_t port);

    ///@brief 只给出ip，在bind中表示随机分配端口
    explicit IPv4Address(const std::string & ipv4_str);

    ~IPv4Address() override = default;

    /// @brief 获取协议族
    [[nodiscard]] sa_family_t GetFamily() const override { return m_address.sin_family; }

    /// @brief @brief 获取 socket 地址
    [[nodiscard]] sockaddr * const GetRawAddr() const override { return (sockaddr *) (&m_address); }

    /// @brief @brief 获取地址长度
    [[nodiscard]] socklen_t GetRawAddrLen() const override { return sizeof(m_address); }

    /// @brief 获取端口号
    [[nodiscard]] uint16_t GetPort() const override { return ntohs(m_address.sin_port); }

    std::string GetPortStr() const override { return std::to_string(ntohs(m_address.sin_port)); };

    /// @brief 设置端口号
    void SetPort(uint16_t port) override { m_address.sin_port = port; }

    uint32_t GetIP() const override  { return m_address.sin_addr.s_addr; }

    /// @brief 转换为字符串
    std::string GetIPStr() const override { return std::string{inet_ntoa(m_address.sin_addr)}; }

    std::string ToString() const override;

private:
    sockaddr_in m_address;
};




class IPv6Address final : public IPAddress
{
public:
    using ptr = std::shared_ptr<IPv4Address>;

    IPv6Address(): m_address{} {}

    explicit IPv6Address(const sockaddr_in6 *addr) : m_address{*addr} {}

    IPv6Address(const std::string & ipv6_str, uint16_t port);

    ~IPv6Address() override = default;

    /// @brief 获取协议族
    [[nodiscard]] sa_family_t GetFamily() const override { return m_address.sin6_family; }

    /// @brief @brief 获取 socket 地址
    [[nodiscard]] sockaddr * const GetRawAddr() const override { return (sockaddr *)&m_address; }

    /// @brief @brief 获取地址长度
    [[nodiscard]] socklen_t GetRawAddrLen() const override { return sizeof m_address;}

    /// @brief 获取端口号
    [[nodiscard]] uint16_t GetPort() const override { return ntohs(m_address.sin6_port); }

    /// @brief 设置端口号
    void SetPort(uint16_t port) override { m_address.sin6_port = port; }

    virtual uint32_t GetIP() const override;

    virtual std::string GetPortStr() const override;

    /// @brief 转换为字符串
    std::string GetIPStr() const override;

    virtual std::string ToString() const override;

private:
    sockaddr_in6 m_address;
};




}

