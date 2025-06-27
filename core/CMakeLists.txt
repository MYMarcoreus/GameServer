cmake_minimum_required(VERSION 3.22)
project(GameServer)
set(CMAKE_CXX_STANDARD 20)


set(LocalSrc
        UserConnection.cpp
        codec/ProtobufTcpCodec.cpp
        codec/ProtobufDispatcher.cpp
        codec/MessageHeader.cpp
        codec/ProtobufUdpCodec.cpp
        redis/RedisClient.cpp
        redis/RedisPool.cpp
        rpc/RpcCodec.cpp
        rpc/RpcChannel.cpp
        rpc/RpcHeader.cpp
        rpc/RpcServer.cpp
        zookeeper/ZKClient.cpp
        mysql/MySqlPool.cpp
)

#! 给本地相对路径添加前缀，变为绝对路径
foreach(LocalFile ${LocalSrc})
    list(PREPEND FullLocalSrc "${CMAKE_CURRENT_SOURCE_DIR}/${LocalFile}")
endforeach()

#! 使用PARENT_SCOPE关键字是的子目录可以更改父目录的变量：这里是添加子目录源文件到父目录的变量中
set(COMMON_SOURCES ${COMMON_SOURCES} ${FullLocalSrc} PARENT_SCOPE)