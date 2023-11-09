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

message(${ProtocCmd} ${ProtocArgs} ${ProtocFiles})
execute_process(
    COMMAND ${ProtocCmd} ${ProtocArgs} ${ProtocFiles}
    WORKING_DIRECTORY ${ProtocDirname}
)

file(GLOB ProtocSrcs ${ProtocDirname}/*.cc )


include_directories(
    base/config
    game
    core
    base/public
    base/utils
    base/log
    protobuf
    base/net
)


set(COMMON_SOURCES
        ${ProtocSrcs}
#        core/LinuxServer.cpp
#        core/LinuxServer_Inner.cpp
        core/UserBaseData.cpp

        base/log/log.cpp
        base/net/IPAddress.cpp
        base/utils/util_functions.cpp
        base/public/md5/md5.cpp
        base/public/tinyxml/tinyxml2.cpp
        base/utils/status/Status.cpp
        base/config/ConfigManager.cpp
        base/config/AppXmlConfig.cpp
        base/config/LogXmlConfig.cpp
        base/config/RemoteXmlConfig.cpp
        base/utils/BoundedQueue.hpp
        base/utils/ThreadPool.cpp

        base/net/Channel.cpp
        base/net/EpollPoller.cpp
        base/net/EventLoop.cpp
        base/net/Poller.cpp
        base/net/NewDefaultPoller.cpp
        base/net/Timestamp.cpp
        base/net/TimerManager.cpp
        base/net/Timer.cpp
        base/net/EventLoopThread.cpp
        base/net/Acceptor.cpp
        base/net/Socket.cpp
        base/net/TcpConnection.cpp
        base/net/Buffer.cpp
        base/net/TcpServer.cpp
        base/net/EventLoopThreadPool.cpp
        base/net/Connector.cpp
        base/net/TcpClient.cpp
        base/net/SocketApiWrapper.cpp
        base/net/SocketApiWrapper.h
        base/net/SelectPoller.cpp
        base/net/SelectPoller.h

        core/codec/ProtobufCodec.cpp
        core/codec/ProtobufDispatcher.cpp
        core/codec/MessageHeader.cpp

        core/WindowsGameServer.cpp
        core/WindowsGameServer.h


        #        game/GameManager.cpp
        #        game/GameNotifier.cpp
        #        game/GamePlayerManager.cpp
        #        game/GameTestManager.cpp
)

add_library(CommonLibrary ${COMMON_SOURCES})

add_executable(codec_client test/test_codec_client.cpp)
add_executable(codec_server test/test_codec_server.cpp)


if(CMAKE_HOST_SYSTEM_NAME MATCHES Windows)
    target_link_libraries( CommonLibrary
            ${PROTOBUF_LIBRARIES}
            ws2_32.lib
            protobuf::libprotoc
            protobuf::libprotobuf
            protobuf::libprotobuf-lite
    )
else()
    target_link_libraries( CommonLibrary
            ${PROTOBUF_LIBRARIES}
            protobuf
            pthread
    )
endif()


#! -lprotobuf：必须在add_executable后面，因为是在可执行文件生成后，再将protobuf库链接的
target_link_libraries(
        codec_client
        CommonLibrary
)

target_link_libraries(
        codec_server
        CommonLibrary
)

#add_executable( test_log
#        test/test_log.cpp
#        base/log/log.cpp
#        base/net/IPAddress.cpp
#        base/utils/util_functions.cpp
#        base/public/md5/md5.cpp
#        base/public/tinyxml/tinyxml2.cpp
#        base/utils/status/Status.cpp
#        base/config/ConfigManager.cpp
#        base/config/AppXmlConfig.cpp
#        base/config/LogXmlConfig.cpp
#        base/config/RemoteXmlConfig.cpp
#        base/utils/BoundedQueue.hpp
#        base/utils/ThreadPool.cpp
#)
#target_link_libraries(test_log ws2_32.lib)