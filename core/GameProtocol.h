#ifndef ____GAMEPROTOCOL_H
#define ____GAMEPROTOCOL_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <cstdlib>
#include <random>
#include "UserBuffer.h"

/*
包构成：
    消息头(checkCode: 8B)：游戏协议
        1、：      char m_CheckCode[2]
        2、消息总长度(4B)： int32_t m_FullLength
        3、指令(2B)：      uint16_t m_TypeNameLength
    消息体(Body)：游戏数据(不是必须的)
*/


/*
客户端 --建立TCP连接--> 服务器
客户端 <--接收连接，并发送随机生成的异或码-- 服务器
客户端 --接收异或验证码（接下来的所有数据都需要用异或码进行异或加密后再发送），然后发送md5加密的安全验证数据--> 服务器
客户端 <--接收md5加密的数据，解密之，进行安全验证，然后发送验证结果-- 服务器
客户端 --接收安全验证结果，发送登录请求--> 服务器
 ---------------业务层--------------------
客户端 <--处理登录请求，若允许登录，则初始化游戏用户数据(或者读取保存的用互数据)-- 服务器
客户端...获取用户数据，赋给游戏对象
接下来客户端所做的：
    1、接收收到他人数据，同步到游戏中
    2、发送因自己的控制所产生的数据（因此需要在Controller中调用发送自己数据的函数）
    3、客户端每次Update都会发送心跳包，因此服务器需要接收之，然后也发送一个心跳包
*/


namespace yy::core
{

// 服务器套接字状态
enum class E_ServerSocketState: int8_t  // C++11特性，指定enum类型的大小
{
    eFree       = 0, // 空闲，无连接(要么已关闭连接、要么未连接)
    eConnected  = 1, // 已连接，等待安全验证
    eSecure     = 2, // 已连接，通过安全验证
    eLoggedIn   = 3, // 已登录
    eNeedSave   = 4, //
    // eSaving     = 5  //
};


// 包的类型(包的指令)：2字节
//      最高位为1：服务器指令
//      最高位为0：业务层指令
enum class E_PackageCommand : uint16_t
{
    /* 业务层的命令 */
    eTest            = 1,
    eLogin           = 1000,
    eMove            = 2000,
    eGetPlayerData   = 3000,
    eLeave           = 4000,
    eJumpAndGravity  = 5000,

    /* 服务器的命令 */
    eHeart    = 65000,  // 验证对端的连接是否存在
    eXor      = 65531,  // 发送给客户端的异或码
    eSecurity = 65532   // 安全验证
};

/*
使用单字节对齐的方式的作用：
    1、可以节约内存空间，但会造成数据存取效率上的损失；
    2、在网络字节流传输结构体时，可以保证传输的是连续的字节流，保证接收方接收到一致的数据。
*/
#pragma pack(push, packing) // 保存当前字节对齐状态
#pragma pack(1) // 设为单字节对齐

/*************************** 游戏协议消息头 ***************************/
struct PackageHead
{
    char     check_code[2]; // 用于验证该包是否是我们规定的游戏协议包
    uint32_t length;        // 指示整个包的长度
    uint16_t cmd;           // 指示消息体中是何种数据，该如何处理之

    PackageHead() = default;
    ~PackageHead() = default;

    void set_check_code(const void * const value, uint8_t xorCode) {
        check_code[0] = static_cast<char>(((char*)value)[0] ^ xorCode);
        check_code[1] = static_cast<char>(((char*)value)[1] ^ xorCode);
    }

    void set_length(uint32_t      value, uint8_t xorCode) { length = value ^ xorCode; }
    void set_length(const void *  value, uint8_t xorCode) { length = *(uint32_t *)value ^ xorCode; }
    void set_cmd(uint16_t         value, uint8_t xorCode) { cmd = value ^ xorCode; }
    void set_cmd(const void *     value, uint8_t xorCode) { cmd = *(uint16_t*)value ^ xorCode; }
    void set_cmd(E_PackageCommand value, uint8_t xorCode) { cmd = (uint16_t)value ^ xorCode; }

    /// @brief 从C数组中读入未解密的数据，并解密
    bool ReadFromBuffer(UserBuffer & buf, uint8_t xorCode)
    {
        // 读入未解密数据
        if(!buf.WriteToStruct(*this)) {
            return false;
        }

        // 解密数据
        set_check_code(check_code, xorCode);
        set_length(length, xorCode);
        set_cmd(cmd, xorCode);

        return true;
    }

    [[nodiscard]] size_t CalcBodyLen() const { return length - sizeof(PackageHead); }
};

/*************************** eXor ***************************/
// 随机产生一个异或码
static uint8_t genXorCode()
{
    std::mt19937  eng{std::random_device{}() }; // 真随机数
    static std::uniform_int_distribution<uint8_t> dis(1, 125); // [1, 125]
    uint8_t gen_val = dis(eng);

    return gen_val;
};


#pragma pack(pop, packing) // 恢复字节对齐状态


}
#endif //____GAMEPROTOCOL_H
