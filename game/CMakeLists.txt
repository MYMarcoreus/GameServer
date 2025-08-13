set(LocalSrc
        rpc_clients/CenterRpcClient.cpp
        rpc_clients/LogicRpcClient.cpp
        rpc_clients/AccountRpcClient.cpp
        rpc_clients/GateRpcClient.cpp
        logic/LogicServerManager.cpp
        logic/GameService.cpp
        logic/TestService.cpp
        logic/Room.cpp
        logic/Player.cpp
        logic/RoomManager.cpp
        account/AccountServerManager.cpp
        account/AccountServiceRpc_Impl.cpp
        center/CenterServiceRpc_Impl.cpp
        center/CenterServerManager.cpp
        center/RoomInfoController.cpp
        center/LogicServerController.cpp
        center/LogicInfoController.cpp
        account/AccountRedisDAO.cpp
        account/AccountData.cpp
        account/AccountMysqlDAO.cpp
        center/CenterRedisDAO.cpp
        logic/LogicRedisDAO.cpp
        gate/GateRedisDAO.cpp
        gate/GateServerManager.cpp
        gate/GateServiceRpc_Impl.cpp
        gate/ForwardManager.cpp
)

#! 给本地相对路径添加前缀，变为绝对路径
foreach(LocalFile ${LocalSrc})
    list(PREPEND FullLocalSrc "${CMAKE_CURRENT_SOURCE_DIR}/${LocalFile}")
endforeach()

#! 使用PARENT_SCOPE关键字是的子目录可以更改父目录的变量：这里是添加子目录源文件到父目录的变量中
set(COMMON_SOURCES ${COMMON_SOURCES} ${FullLocalSrc} PARENT_SCOPE)



add_subdirectory(logic)
