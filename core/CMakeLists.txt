set(LocalSrc
        UserConnection.cpp
        codec/ProtobufDispatcher.cpp
        codec/MessageHeader_Name.cpp
        codec/ProtobufTcpCodec_Name.cpp
        codec/ProtobufUdpCodec_Name.cpp
        codec/MessageHeader_Cmd.cpp
        codec/ProtobufTcpCodec_Cmd.cpp
        codec/ProtobufUdpCodec_Cmd.cpp
        redis/RedisClient.cpp
        redis/RedisPool.cpp
        mysql/MySqlPool.cpp
        mysql/MySqlClient.cpp
        rpc/RpcCodec.cpp
        rpc/RpcConnection.cpp
        rpc/RpcHeader.cpp
        rpc/RpcServer.cpp
        rpc/RpcControllerImpl.cpp
        rpc/RpcStubConnectionPool.hpp
        zookeeper/ZkClient.cpp
        zookeeper/ZkServiceClient.cpp
        FrontendServer.cpp
)

#! 给本地相对路径添加前缀，变为绝对路径
foreach(LocalFile ${LocalSrc})
    list(PREPEND FullLocalSrc "${CMAKE_CURRENT_SOURCE_DIR}/${LocalFile}")
endforeach()

#! 使用PARENT_SCOPE关键字是的子目录可以更改父目录的变量：这里是添加子目录源文件到父目录的变量中
set(COMMON_SOURCES ${COMMON_SOURCES} ${FullLocalSrc} PARENT_SCOPE)