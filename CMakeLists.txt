cmake_minimum_required(VERSION 3.22)
project(GameServer)

# ---------- ccache 编译缓存（大幅加速增量构建） ----------
# 需要在创建任何 target 之前设置；未安装 ccache 时自动跳过。
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    message(STATUS "使用 ccache 编译缓存: ${CCACHE_PROGRAM}")
else()
    message(STATUS "未找到 ccache，跳过编译缓存（可用 sudo apt install ccache 启用）")
endif()

message(STATUS "CMAKE_TOOLCHAIN_FILE: ${CMAKE_TOOLCHAIN_FILE}")

enable_testing()
find_package(GTest CONFIG REQUIRED)

# -std=c++23 (启用 std::expected 等 C++23 标准库设施；需 GCC 13+ / Clang 16+)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DUUID_ENABLE_STL_HASH)
find_package(stduuid CONFIG REQUIRED)

find_package(redis++ REQUIRED)
find_package(unofficial-mysql-connector-cpp CONFIG REQUIRED)
#find_package(unofficial-zookeeper CONFIG REQUIRED)

add_definitions(-DTHREADED)
include_directories(${Zookeeper_INCLUDE_DIRS})
link_directories(${Zookeeper_LIBRARIES})

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

# ---------- 编译提速/省内存（GCC/Clang） ----------
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # -pipe：用管道传递编译中间数据，减少临时文件磁盘 IO
    add_compile_options(-pipe)

    if(CMAKE_BUILD_TYPE MATCHES "Debug")
        # Debug 默认用 -g1（行号+函数名，不含局部变量类型信息）：
        # 大幅降低每个编译任务的内存占用与耗时，避免 WSL/小内存机器上并行编译 OOM。
        # 需要完整调试信息时：cmake -DYY_FULL_DEBUG=ON（或 -DCMAKE_CXX_FLAGS_DEBUG="-g"）
        option(YY_FULL_DEBUG "Debug 构建使用完整调试信息 -g（默认 -g1 以省内存/加速）" OFF)
        if(YY_FULL_DEBUG)
            set(CMAKE_CXX_FLAGS_DEBUG "-g")
        else()
            set(CMAKE_CXX_FLAGS_DEBUG "-g1")
        endif()
    endif()
endif()

# 查找 protobuf（Windows 下使用 CONFIG 模式）
if(WIN32)
    find_package(Protobuf CONFIG REQUIRED)
else()
    find_package(Protobuf REQUIRED)
endif()

#! 自动执行终端命令生成C++代码文件
set(ProtocDirname ${CMAKE_SOURCE_DIR}/protobuf)

include(cmake/ProtocGenCpp.cmake)
ProtoGenCpp(${ProtocDirname})

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
        net
        core
        core/actor
        core/frontend
        core/redis
        core/mysql
        core/zk
        core/rpc
        protobuf
        game
        game/logic
        game/gate
        game/account
        game/rpc_clients
        ${Protobuf_INCLUDE_DIRS}
)


set(COMMON_SOURCES
        base/log/ILogAppender.cpp
)
# 调用子目录下的cmake
add_subdirectory(base)
add_subdirectory(net)
add_subdirectory(core)
add_subdirectory(protobuf)
add_subdirectory(game)
add_subdirectory(tests)
add_subdirectory(benchmark)


#! 制作 libCommonLibrary.a 静态库
add_library(CommonLibrary STATIC ${COMMON_SOURCES}) # STATIC替换为SHARED就为动态库，生成 libCommonLibrary.so

# ---------- 预编译头(PCH)加速编译 ----------
# 将标准库/protobuf/基础项目头预编译一次，避免每个 TU 重复解析（编译慢的主因）。
# 只作用于 CommonLibrary（包含全部业务 .cpp，收益最大）；可执行文件 main_*.cpp 很小，无需 PCH。
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_precompile_headers(CommonLibrary PRIVATE base/pch.h)
endif()

#! 添加系统动态库
if(CMAKE_HOST_SYSTEM_NAME MATCHES Windows)
    message("Linking Librarys in Windows!!")
    #! 有target前缀的就是链接动态库
    target_link_libraries( CommonLibrary ws2_32.lib )
else()
    target_link_libraries( CommonLibrary
            PUBLIC pthread
    )
endif()

target_link_libraries(CommonLibrary
            PUBLIC ${PROTOBUF_LIBRARIES}
            protobuf::libprotoc
            protobuf::libprotobuf
            protobuf::libprotobuf-lite
            zookeeper_mt
#            unofficial::zookeeper::zookeeper
            redis++::redis++_static
            PRIVATE unofficial::mysql-connector-cpp::connector
            resolv
            stduuid
)

#! 添加多个可执行文件，可在clion选择一个来构建/执行
add_executable(LogicServer game/logic/main_logic.cpp)
#! 在cmake中指定要链接的动态库的时候，应该将命令写到生成了可执行文件之后，因为是在可执行文件生成后，再将protobuf库链接的
target_link_libraries(LogicServer CommonLibrary)

add_executable(GateServer game/gate/main_gate.cpp)
target_link_libraries(GateServer CommonLibrary)

add_executable(AccountServer game/account/main_account.cpp)
target_link_libraries(AccountServer CommonLibrary)

add_executable(CenterServer game/center/main_center.cpp)
target_link_libraries(CenterServer CommonLibrary)