#ifndef ____GAMEPROTOCOL_H
#define ____GAMEPROTOCOL_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <cstdlib>
#include <random>

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




}
#endif //____GAMEPROTOCOL_H

