set(LocalSrc
        frontend/UserConnection.cpp
        frontend/ProtobufDispatcher.cpp
        frontend/MessageHeader_Name.cpp
        frontend/ProtobufTcpCodec_Name.cpp
        frontend/ProtobufUdpCodec_Name.cpp
        frontend/MessageHeader_Cmd.cpp
        frontend/ProtobufTcpCodec_Cmd.cpp
        frontend/ProtobufUdpCodec_Cmd.cpp
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
        zk/ZkClient.cpp
        zk/ZkServiceClient.cpp
        frontend/FrontendServer.cpp
)

#! 给本地相对路径添加前缀，变为绝对路径
foreach(LocalFile ${LocalSrc})
    list(PREPEND FullLocalSrc "${CMAKE_CURRENT_SOURCE_DIR}/${LocalFile}")
endforeach()

#! 使用PARENT_SCOPE关键字是的子目录可以更改父目录的变量：这里是添加子目录源文件到父目录的变量中
set(COMMON_SOURCES ${COMMON_SOURCES} ${FullLocalSrc} PARENT_SCOPE)