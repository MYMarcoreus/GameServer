set(LocalSrc
        rpc_clients/CenterRpcClient.cpp
        rpc_clients/LogicRpcClient.cpp
        rpc_clients/AccountRpcClient.cpp
        logic/LogicServerManager.cpp
        logic/GameService.cpp
        logic/TestService.cpp
        logic/Room.cpp
        logic/Player.cpp
        logic/RoomManager.cpp
        account/AccountServerManager.cpp
        account/AccountRpcServiceImpl.cpp
        gate/GateServerManager.cpp
        center/CenterRpcServiceImpl.cpp
        center/CenterServerManager.cpp
        center/RoomInfoController.cpp
        center/LogicServerController.cpp
        center/LogicInfoController.cpp
)

#! 给本地相对路径添加前缀，变为绝对路径
foreach(LocalFile ${LocalSrc})
    list(PREPEND FullLocalSrc "${CMAKE_CURRENT_SOURCE_DIR}/${LocalFile}")
endforeach()

#! 使用PARENT_SCOPE关键字是的子目录可以更改父目录的变量：这里是添加子目录源文件到父目录的变量中
set(COMMON_SOURCES ${COMMON_SOURCES} ${FullLocalSrc} PARENT_SCOPE)



add_subdirectory(logic)
