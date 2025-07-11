cmake_minimum_required(VERSION 3.22)
project(GameServer)
set(CMAKE_CXX_STANDARD 20)


set(LocalSrc
        logic/LogicServerManager.cpp
        logic/RoomService.cpp
        logic/TestService.cpp
        login/LoginMaganer.cpp
        login/AccountRpcService.cpp
        gate/GateServerManager.cpp
        gate/AccountRpcClient.cpp
        gate/AccountRpcClient.h
)

#! 给本地相对路径添加前缀，变为绝对路径
foreach(LocalFile ${LocalSrc})
    list(PREPEND FullLocalSrc "${CMAKE_CURRENT_SOURCE_DIR}/${LocalFile}")
endforeach()

#! 使用PARENT_SCOPE关键字是的子目录可以更改父目录的变量：这里是添加子目录源文件到父目录的变量中
set(COMMON_SOURCES ${COMMON_SOURCES} ${FullLocalSrc} PARENT_SCOPE)



add_subdirectory(logic)
