cmake_minimum_required(VERSION 3.22)
project(GameServer)

# -std=c++20
set(CMAKE_CXX_STANDARD 20)


# -DCMAKE_TOOLCHAIN_FILE=C:\Users\Lenovo\.vcpkg-clion\vcpkg\scripts\buildsystems\vcpkg.cmake

# 判断平台
if(CMAKE_HOST_SYSTEM_NAME MATCHES Windows)
    message(STATUS "Platform: Windows")
    add_definitions(-D____WINDOWS)
    find_package(Protobuf CONFIG REQUIRED)

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /source-charset:utf-8")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /source-charset:utf-8")
    #    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /execution-charset:utf-8")
    #    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /execution-charset:utf-8")
elseif(CMAKE_HOST_SYSTEM_NAME MATCHES Linux)
    message(STATUS "Platform: Linux")
    add_definitions(-D____LINUX)
    #! 加载Protobuf包
    find_package(Protobuf REQUIRED) #  CONFIG
elseif(CMAKE_HOST_SYSTEM_NAME MATCHES Darwin)
    message(STATUS "Platform: Darwin(MacOS)")
    add_definitions(-D____MACOS)
else()
    message(STATUS "unknown platform")
endif()


# 如果是以Debug模式构建的就定义一个名为____DEBUG的宏定义
if(CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-D____DEBUG)
endif()



message(STATUS "CMAKE_CXX_COMPILER_ID = ${CMAKE_CXX_COMPILER_ID}")

if(CMAKE_CXX_COMPILER_ID MATCHES GNU)
    message(STATUS "Using GCC compiler")
    add_definitions(-D____GNUC)

elseif(CMAKE_CXX_COMPILER_ID MATCHES MSVC)
    message(STATUS "Using MSVC compiler")
    add_definitions(-D____MSVC)

    add_compile_options(/MP)
    add_compile_options("/Zc:__cplusplus")
endif()





#! 自动执行终端命令生成C++代码文件
set(ProtocDirname ${CMAKE_SOURCE_DIR}/protobuf)

# 不带引号时，ProtocArgs后面的参数是数组，如果带引号则会变成带;的字符串
set(ProtocCmd ${Protobuf_PROTOC_EXECUTABLE})
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

message(STATUS "Protobuf_VERSION = ${Protobuf_VERSION}")
message(STATUS "PROTOBUF_INCLUDE_DIRS = ${PROTOBUF_INCLUDE_DIRS}")
message(STATUS "Protobuf_FOUND = ${Protobuf_FOUND}")

message(STATUS "Protobuf_INCLUDE_DIR = ${Protobuf_INCLUDE_DIR}")
message(STATUS "Protobuf_LIBRARY = ${Protobuf_LIBRARY}")
message(STATUS "Protobuf_PROTOC_LIBRARY = ${Protobuf_PROTOC_LIBRARY}")
message(STATUS "Protobuf_LIBRARY_DEBUG = ${Protobuf_LIBRARY_DEBUG}")
message(STATUS "Protobuf_PROTOC_LIBRARY_DEBUG = ${Protobuf_PROTOC_LIBRARY_DEBUG}")
message(STATUS "Protobuf_LITE_LIBRARY = ${Protobuf_LITE_LIBRARY}")
message(STATUS "Protobuf_LITE_LIBRARY_DEBUG = ${Protobuf_LITE_LIBRARY_DEBUG}")


message(STATUS "Protobuf_LIBRARIES = ${Protobuf_LIBRARIES}")
message(STATUS "Protobuf_PROTOC_LIBRARIES = ${Protobuf_PROTOC_LIBRARIES}")
message(STATUS "Protobuf_LITE_LIBRARIES = ${Protobuf_LITE_LIBRARIES}")


link_directories(${PROTOBUF_LIBRARIES})


include_directories(
        base
        base/config
        base/public
        base/utils
        base/log
        base/net
        game
        core
        protobuf
        ${Protobuf_INCLUDE_DIRS}
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

set(COMMON_SOURCES
        base/net/PriorityQueueTimerManager.cpp
        base/net/NewDefaultTimerManager.cpp
)
# 调用子目录下的cmake
add_subdirectory(base)
add_subdirectory(core)
add_subdirectory(game)
add_subdirectory(protobuf)

#message(COMMON_SOURCES = ${COMMON_SOURCES})



#! 制作 libCommonLibrary.a 静态库
add_library(CommonLibrary STATIC ${COMMON_SOURCES}) # STATIC替换为SHARED就为动态库，生成 libCommonLibrary.so


#! 添加多个可执行文件，可在clion选择一个来构建/执行
add_executable(codec_client test/test_codec_client.cpp)
add_executable(codec_server test/test_codec_server.cpp)
add_executable(main  main.cpp)
add_executable(test testtt.cpp)
add_executable(echo_server test/test_echo.cpp)

#! 添加系统动态库
if(CMAKE_HOST_SYSTEM_NAME MATCHES Windows)
    message("Linking Librarys in Windows!!")
    #! 有target前缀的就是链接动态库
    target_link_libraries( CommonLibrary
            PRIVATE protobuf::libprotoc
            protobuf::libprotobuf
            protobuf::libprotobuf-lite
            ws2_32.lib
    )
else()
    target_link_libraries( CommonLibrary
            PUBLIC ${PROTOBUF_LIBRARIES}
            PUBLIC pthread
            #            PUBLIC protobuf
            #            PUBLIC mysqlcppconn
            protobuf::libprotoc
            protobuf::libprotobuf
            protobuf::libprotobuf-lite
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


target_link_libraries(
        echo_server
        CommonLibrary
)

target_link_libraries(
        test
        CommonLibrary
)

