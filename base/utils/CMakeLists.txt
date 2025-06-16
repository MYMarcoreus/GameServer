cmake_minimum_required(VERSION 3.22)
project(GameServer)
set(CMAKE_CXX_STANDARD 20)

set(LocalSrc
        util_functions.cpp
        ThreadPool.cpp
        status/Status.cpp
        SequentialBuffer.cpp
        SequentialBuffer.cpp
        RingBuffer.cpp
)

#! 给本地相对路径添加前缀，变为绝对路径
foreach(LocalFile ${LocalSrc})
    list(APPEND FullLocalSrc "${CMAKE_CURRENT_SOURCE_DIR}/${LocalFile}")
endforeach()

#! 使用PARENT_SCOPE关键字是的子目录可以更改父目录的变量：这里是添加子目录源文件到父目录的变量中
#set(COMMON_SOURCES ${COMMON_SOURCES} ${FullLocalSrc} PARENT_SCOPE)
set(FullLocalSrc ${FullLocalSrc} PARENT_SCOPE)
