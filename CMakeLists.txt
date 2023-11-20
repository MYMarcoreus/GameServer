cmake_minimum_required(VERSION 3.22)
project(GameServer)

# -std=c++20
set(CMAKE_CXX_STANDARD 20)

# -DCMAKE_TOOLCHAIN_FILE=C:\Users\Lenovo\.vcpkg-clion\vcpkg\scripts\buildsystems\vcpkg.cmake

# 判断平台
if(CMAKE_HOST_SYSTEM_NAME MATCHES Windows)
    message(">> Platform: Windows")
    add_definitions(-D____WINDOWS)
elseif(CMAKE_HOST_SYSTEM_NAME MATCHES Linux)
    message(">> Platform: Linux")
    add_definitions(-D____LINUX)
elseif(CMAKE_HOST_SYSTEM_NAME MATCHES Darwin)
    message(">> Platform: Darwin(MacOS)")
    add_definitions(-D____MACOS)
else()
    message(">> unknow.")
endif()

# 如果是以Debug模式构建的就定义一个名为____DEBUG的宏定义
if(CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-D____DEBUG)
endif()




#! 加载Protobuf包
find_package(Protobuf REQUIRED) #  CONFIG


#! 自动执行终端命令生成C++代码文件
set(ProtocDirname ${CMAKE_SOURCE_DIR}/protobuf)

# 不带引号时，ProtocArgs后面的参数是数组，如果带引号则会变成带;的字符串
set(ProtocCmd protoc)
set(ProtocArgs # 不能带引号！
    --proto_path=${ProtocDirname}
    --cpp_out=${ProtocDirname}
    --experimental_allow_proto3_optional
)
file(GLOB ProtocFiles ${ProtocDirname}/*.proto )

message(execute_process: ${ProtocCmd} ${ProtocArgs} ${ProtocFiles})

execute_process(
    COMMAND ${ProtocCmd} ${ProtocArgs} ${ProtocFiles}
    WORKING_DIRECTORY ${ProtocDirname}
)

message("PROTOBUF_LIBRARIES = ${PROTOBUF_LIBRARIES}")



include_directories(
    base/config
    base/public
    base/utils
    base/log
    base/net
    game
    core
    protobuf
)

#file(GLOB ProtocSrcs ${ProtocDirname}/*.cc )
#

#set(COMMON_SOURCES
#        ${ProtocSrcs}
#        base/public/md5/md5.cpp
#        base/public/tinyxml/tinyxml2.cpp
#        base/log/log.cpp
#        base/config/ConfigManager.cpp
#        base/config/AppXmlConfig.cpp
#        base/config/LogXmlConfig.cpp
#        base/config/RemoteXmlConfig.cpp
#        base/utils/util_functions.cpp
#        base/utils/BoundedQueue.hpp
#        base/utils/ThreadPool.cpp
#        base/utils/status/Status.cpp
#        base/net/IPAddress.cpp
#        base/net/Channel.cpp
#        base/net/EpollPoller.cpp
#        base/net/EventLoop.cpp
#        base/net/Poller.cpp
#        base/net/NewDefaultPoller.cpp
#        base/net/Timestamp.cpp
#        base/net/TimerManager.cpp
#        base/net/Timer.cpp
#        base/net/EventLoopThread.cpp
#        base/net/Acceptor.cpp
#        base/net/Socket.cpp
#        base/net/TcpConnection.cpp
#        base/net/Buffer.cpp
#        base/net/TcpServer.cpp
#        base/net/EventLoopThreadPool.cpp
#        base/net/Connector.cpp
#        base/net/TcpClient.cpp
#        base/net/SocketApiWrapper.cpp
#        base/net/SelectPoller.cpp
#        base/net/FullDuplexPipe.cpp
#        core/codec/ProtobufCodec.cpp
#        core/codec/ProtobufDispatcher.cpp
#        core/codec/MessageHeader.cpp
#        core/GameServer.cpp
#        core/UserBaseData.cpp
#        game/GameManager.cpp
#        game/GamePlayerManager.cpp
#        game/GameTestManager.cpp
#)

#file(GLOB_RECURSE COMMON_SOURCES
#        base/*.cpp
#        core/*.cpp
#        game/*.cpp
#        protobuf/*.cc
#)

set(COMMON_SOURCES)
# 调用子目录下的cmake
add_subdirectory(base)
add_subdirectory(core)
add_subdirectory(game)
add_subdirectory(protobuf)

message(COMMON_SOURCES = ${COMMON_SOURCES})



#! 制作 libCommonLibrary.a 静态库
add_library(CommonLibrary STATIC ${COMMON_SOURCES}) # STATIC替换为SHARED就为动态库，生成 libCommonLibrary.so

#! 添加多个可执行文件，可在clion选择一个来构建/执行
add_executable(codec_client test/test_codec_client.cpp)
add_executable(codec_server test/test_codec_server.cpp)
add_executable(main main.cpp)

#! 添加系统动态库
if(CMAKE_HOST_SYSTEM_NAME MATCHES Windows)
    #! 有target前缀的就是链接动态库
    target_link_libraries( CommonLibrary
            PUBLIC ${PROTOBUF_LIBRARIES}
            PUBLIC ws2_32.lib
            PUBLIC protobuf::libprotoc
            PUBLIC protobuf::libprotobuf
            PUBLIC protobuf::libprotobuf-lite
    )
else()
    target_link_libraries( CommonLibrary
            PUBLIC ${PROTOBUF_LIBRARIES}
            PUBLIC protobuf
            PUBLIC pthread
    )
endif()


#! 在cmake中指定要链接的动态库的时候，应该将命令写到生成了可执行文件之后，因为是在可执行文件生成后，再将protobuf库链接的
target_link_libraries(
        codec_client
        CommonLibrary
)

target_link_libraries(
        codec_server
        CommonLibrary
)

target_link_libraries(
        main
        CommonLibrary
)

