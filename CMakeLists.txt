cmake_minimum_required(VERSION 3.22)
project(GameServer)

message(STATUS "CMAKE_TOOLCHAIN_FILE: ${CMAKE_TOOLCHAIN_FILE}")

# -std=c++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#! 查找 Lua
find_package(Lua REQUIRED)
message(STATUS "LUA_INCLUDE_DIR: ${LUA_INCLUDE_DIR}")
message(STATUS "LUA_LIBRARIES: ${LUA_LIBRARIES}")
message(STATUS "LUA_EXECUTABLE: ${LUA_EXECUTABLE}")

#! 打印平台和编译器信息
message(STATUS "CMAKE_HOST_SYSTEM_NAME = ${CMAKE_HOST_SYSTEM_NAME}")
message(STATUS "CMAKE_CXX_COMPILER_ID = ${CMAKE_CXX_COMPILER_ID}")


#! 根据平台设置宏定义和依赖
if(CMAKE_HOST_SYSTEM_NAME MATCHES "Windows")
    message(STATUS "Platform: Windows")
    add_definitions(-D____WINDOWS)
    add_definitions(-DFD_SETSIZE=1024) # Windows下select最多处理64个套接字
    add_definitions(-DNOMINMAX) # Windows下select最多处理64个套接字
elseif(CMAKE_HOST_SYSTEM_NAME MATCHES "Linux")
    message(STATUS "Platform: Linux")
    add_definitions(-D____LINUX)
elseif(CMAKE_HOST_SYSTEM_NAME MATCHES "Darwin")
    message(STATUS "Platform: macOS")
    add_definitions(-D____MACOS)
else()
    message(WARNING "Unknown platform: ${CMAKE_HOST_SYSTEM_NAME}")
endif()

#! 根据编译器设置参数
if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    message(STATUS "Using MSVC compiler")
    add_definitions(-D____MSVC)
    add_compile_options(/MP)
    add_compile_options("/Zc:__cplusplus")
    # 防止控制台输出乱码
    if(CMAKE_BUILD_TYPE MATCHES "Debug")
        # 调试似乎使用的是Clion自带的控制台，支持UTF-8
        add_compile_options("/source-charset:utf-8" "/execution-charset:utf-8")
    else ()
        # 运行似乎使用的是Windows的控制台，默认编码为GBK
        add_compile_options("/source-charset:utf-8" "/execution-charset:gbk")
    endif ()

elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    message(STATUS "Using GCC compiler")
    add_definitions(-D____GNUC)
    # 防止控制台输出乱码
    if(CMAKE_HOST_SYSTEM_NAME MATCHES "Windows")
        if(CMAKE_BUILD_TYPE MATCHES "Debug")
                add_compile_options(-finput-charset=utf-8 -fexec-charset=utf-8)
        else ()
            add_compile_options(-finput-charset=utf-8 -fexec-charset=gbk)
        endif ()
    endif ()

endif()

# 添加 Debug 构建宏
if(CMAKE_BUILD_TYPE MATCHES "Debug")
    add_definitions(-D____DEBUG)
endif()

# 查找 protobuf（Windows 下使用 CONFIG 模式）
if(WIN32)
    find_package(Protobuf CONFIG REQUIRED)
else()
    find_package(Protobuf REQUIRED)
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

message(STATUS "Protobuf_FOUND = ${Protobuf_FOUND}")
message(STATUS "Protobuf_VERSION = ${Protobuf_VERSION}")
message(STATUS "PROTOBUF_INCLUDE_DIRS = ${PROTOBUF_INCLUDE_DIRS}")
message(STATUS "Protobuf_INCLUDE_DIR = ${Protobuf_INCLUDE_DIR}")
message(STATUS "Protobuf_LIBRARY = ${Protobuf_LIBRARY}")
message(STATUS "Protobuf_PROTOC_LIBRARY = ${Protobuf_PROTOC_LIBRARY}")
message(STATUS "Protobuf_LIBRARY_DEBUG = ${Protobuf_LIBRARY_DEBUG}")
message(STATUS "Protobuf_PROTOC_LIBRARY_DEBUG = ${Protobuf_PROTOC_LIBRARY_DEBUG}")
message(STATUS "Protobuf_LITE_LIBRARY = ${Protobuf_LITE_LIBRARY}")
message(STATUS "Protobuf_LITE_LIBRARY_DEBUG = ${Protobuf_LITE_LIBRARY_DEBUG}")
message(STATUS "PROTOBUF_LIBRARIES = ${PROTOBUF_LIBRARIES}")
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
#file(GLOB_RECURSE COMMON_SOURCES
#        base/*.cpp
#        core/*.cpp
#        game/*.cpp
#        protobuf/*.cc
#)

set(COMMON_SOURCES
        base/net/PriorityQueueTimerManager.cpp
        base/net/NewDefaultTimerManager.cpp
        base/log/LogFormatter.cpp
        base/log/LogBufferManager.cpp
        base/log/StdoutLogApeender.h
        base/log/FileLogAppender.h
        base/log/StdoutLogApeender.cpp
        base/log/StdoutLogApeender.h
        base/log/FileLogAppender.cpp
        base/log/FileLogAppender.h
        base/utils/FactoryX.h
        base/utils/ClassRegister.h
        base/utils/SequentialBuffer.cpp
        base/utils/SequentialBuffer.h
        base/log/ILogAppender.h
        base/net/socket_definations.cpp
        base/net/UdpTransport.cpp
        base/net/UdpServer.cpp
        core/codec/ProtobufUdpCodec.cpp
        core/codec/ProtobufUdpCodec.h
        base/net/UdpSession.cpp
        base/net/UdpSession.h
        base/net/NetBuffer.cpp
        base/utils/RingBuffer.cpp
        base/utils/RingBuffer.h
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

